typedef enum RingError {
    RingErrorWouldBlock = -1,
    RingErrorTimedout = -2,
    RingErrorPartialWriteInProgress = -3,
    RingErrorPartialWriteLengthMismatch = -4,
    RingErrorIO = -5,
} RingError;
