#ifndef EX04_QT_IPC_PROTOCOL_H
#define EX04_QT_IPC_PROTOCOL_H

/* Linux user-process IPC only; unrelated to TI SharedRegion/Notify. */
#define EX04_IPC_WRITABLE 0
#define EX04_IPC_READABLE 1
#define EX04_IPC_MESSAGES 10
#define EX04_IPC_TEXT_SIZE 256

typedef struct {
    unsigned int sequence;
    char text[EX04_IPC_TEXT_SIZE];
} Ex04IpcMessage;

#endif
