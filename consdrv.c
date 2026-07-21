#include "define.h"
#include "handler.h"
#include "interrupt.h"
#include "kernel.h"
#include "serial.h"
#include "lib.h"
#include "consdrv.h"

#define CONSDRV_RECV_BUFFER_SIZE 48

/*
 * Static TX storage avoids dynamic allocation in the interrupt handler.
 * The size must be a power of two because indices wrap using a mask.
 */
#define CONSDRV_SEND_BUFFER_SIZE 512
#define CONSDRV_SEND_BUFFER_MASK (CONSDRV_SEND_BUFFER_SIZE - 1)

#if (CONSDRV_SEND_BUFFER_SIZE & CONSDRV_SEND_BUFFER_MASK) != 0
#error CONSDRV_SEND_BUFFER_SIZE must be a power of two
#endif

typedef struct {
    picox_thread_id_t owner;

    char recv_buffer[CONSDRV_RECV_BUFFER_SIZE];
    int recv_length;
    int previous_was_cr;

    uint8_t send_buffer[CONSDRV_SEND_BUFFER_SIZE];
    volatile uint32_t send_head; /* interrupt consumer */
    volatile uint32_t send_tail; /* thread/interrupt producer */
} consreg;

static consreg consoles[CONSDRV_DEVICE_NUM];

static uint32_t send_buffer_used(const consreg *cons)
{
    return (cons->send_tail - cons->send_head) &
           CONSDRV_SEND_BUFFER_MASK;
}

static uint32_t send_buffer_free(const consreg *cons)
{
    return (CONSDRV_SEND_BUFFER_SIZE - 1u) -
           send_buffer_used(cons);
}

static int send_push_raw(consreg *cons, uint8_t value)
{
    uint32_t next =
        (cons->send_tail + 1u) & CONSDRV_SEND_BUFFER_MASK;

    if (next == cons->send_head) {
        return -1;
    }

    cons->send_buffer[cons->send_tail] = value;
    cons->send_tail = next;

    return 0;
}

static int send_required_size(const char *data, int length)
{
    int i;
    int required = 0;

    for (i = 0; i < length; i++) {
        required += (data[i] == '\n') ? 2 : 1;
    }

    return required;
}

/*
 * Add text to the driver's TX ring. LF is converted to CRLF here so that
 * command code can consistently use '\n'.
 */
static int send_push_text(consreg *cons, const char *data, int length)
{
    int i;
    int required;

    required = send_required_size(data, length);

    if ((uint32_t)required > send_buffer_free(cons)) {
        return -1;
    }

    for (i = 0; i < length; i++) {
        if (data[i] == '\n') {
            send_push_raw(cons, (uint8_t)'\r');
        }

        send_push_raw(cons, (uint8_t)data[i]);
    }

    return 0;
}

/*
 * Fill the UART FIFO.  If software data remains, the TX interrupt is left
 * enabled; otherwise it is disabled until new data is queued.
 */
static void send_drain(consreg *cons)
{
    while (cons->send_head != cons->send_tail &&
           serial_is_send_enable()) {
        serial_send_byte(cons->send_buffer[cons->send_head]);
        cons->send_head =
            (cons->send_head + 1u) & CONSDRV_SEND_BUFFER_MASK;
    }

    if (cons->send_head == cons->send_tail) {
        serial_intr_send_disable();
    } else if (!serial_intr_is_send_enable()) {
        serial_intr_send_enable();
    }
}

/* Called from thread context. */
static int send_text(consreg *cons, const char *data, int length)
{
    int result;

    /*
     * RX interrupt echo and this thread both produce TX data, so protect
     * the enqueue operation against UART interrupt preemption.
     */
    INTR_DISABLE();

    result = send_push_text(cons, data, length);

    if (result == 0) {
        send_drain(cons);
    }

    INTR_ENABLE();

    return result;
}

/* Called while UART0 IRQ is already active. */
static void send_text_from_interrupt(
    consreg *cons,
    const char *data,
    int length)
{
    if (send_push_text(cons, data, length) == 0) {
        send_drain(cons);
    }
}

static void send_input_line(consreg *cons)
{
    char *message;
    int size = cons->recv_length + 1;

    message = (char *)picoxs_kmalloc(size);

    memcpy(message, cons->recv_buffer, cons->recv_length);
    message[cons->recv_length] = '\0';

    picoxs_send(MSGBOX_ID_CONSINPUT, size, message);

    cons->recv_length = 0;
}

static void process_received_character(consreg *cons, int c)
{
    /*
     * Treat CR, LF, and CRLF as one Enter key.
     */
    if (c == '\n' && cons->previous_was_cr) {
        cons->previous_was_cr = 0;
        return;
    }

    if (c == '\r') {
        cons->previous_was_cr = 1;
        c = '\n';
    } else {
        cons->previous_was_cr = 0;
    }

    if (c == '\n') {
        send_text_from_interrupt(cons, "\n", 1);
        send_input_line(cons);
        return;
    }

    if (c == 0x03) { /* Ctrl-C */
        cons->recv_length = 0;
        send_text_from_interrupt(cons, "^C\n", 3);
        send_input_line(cons);
        return;
    }

    if (c == '\b' || c == 0x7f) {
        if (cons->recv_length > 0) {
            cons->recv_length--;
            send_text_from_interrupt(cons, "\b \b", 3);
        }

        return;
    }

    if (c >= ' ' && c <= '~') {
        if (cons->recv_length < CONSDRV_RECV_BUFFER_SIZE - 1) {
            char character = (char)c;

            cons->recv_buffer[cons->recv_length++] = character;
            send_text_from_interrupt(cons, &character, 1);
        } else {
            char bell = '\a';
            send_text_from_interrupt(cons, &bell, 1);
        }
    }
}

static void consdrv_interrupt(void)
{
    consreg *cons = &consoles[CONSDRV_DEFAULT_DEVICE];

    if (serial_intr_is_recv() || serial_is_recv_enable()) {
        while (serial_is_recv_enable()) {
            process_received_character(
                cons,
                (int)serial_recv_byte());
        }

        serial_intr_clear_recv();
    }

    if (serial_intr_is_send()) {
        serial_intr_clear_send();
    }

    if (cons->send_head != cons->send_tail) {
        send_drain(cons);
    } else {
        serial_intr_send_disable();
    }
}

static int consdrv_command(
    consreg *cons,
    picox_thread_id_t sender,
    int size,
    char *command)
{
    if (size < 1) {
        return -1;
    }

    switch ((uint8_t)command[0]) {
    case CONSDRV_CMD_USE:
        if (cons->owner != 0 && cons->owner != sender) {
            return -1;
        }

        cons->owner = sender;
        cons->recv_length = 0;
        cons->previous_was_cr = 0;

        serial_intr_recv_enable();
        return 0;

    case CONSDRV_CMD_WRITE:
        if (cons->owner != sender) {
            return -1;
        }

        return send_text(cons, command + 1, size - 1);

    default:
        return -1;
    }
}

int consdrv_main(int argc, char *argv[])
{
    picox_thread_id_t sender;
    char *message;
    int size;
    int index;

    (void)argc;
    (void)argv;

    memset(consoles, 0, sizeof(consoles));

    serial_intr_recv_disable();
    serial_intr_send_disable();

    if (picox_setintr(
            SOFTVEC_TYPE_SERINTR,
            consdrv_interrupt) != 0) {
        picox_sysdown();
    }

    serial_interrupt_enable();

    for (;;) {
        sender = picox_recv(
            MSGBOX_ID_CONSOUTPUT,
            &size,
            &message);

        if (!message || size < 2) {
            if (message) {
                picox_free(message);
            }

            continue;
        }

        index = (uint8_t)message[0];

        if (index >= 0 && index < CONSDRV_DEVICE_NUM) {
            consdrv_command(
                &consoles[index],
                sender,
                size - 1,
                message + 1);
        }

        picox_free(message);
    }

    return 0;
}
