; ==============================================================================
; CCOS Bootloader - Serial Port Constants
; ==============================================================================
; Serial Port (COM1) Constants
%define SERIAL_PORT        0x3F8
%define SERIAL_THR         0x03F8  ; Transmit Holding Register (write)
%define SERIAL_RBR         0x03F8  ; Receive Buffer Register (read)
%define SERIAL_IER         0x03F9  ; Interrupt Enable Register
%define SERIAL_IIR         0x03FA  ; Interrupt Identification Register
%define SERIAL_FCR         0x03FA  ; FIFO Control Register (write)
%define SERIAL_LCR         0x03FB  ; Line Control Register
%define SERIAL_MCR         0x03FC  ; Modem Control Register
%define SERIAL_LSR         0x03FD  ; Line Status Register
%define SERIAL_MSR         0x03FE  ; Modem Status Register
%define SERIAL_DLL         0x03F8  ; Divisor Latch Low (when DLAB=1)
%define SERIAL_DLM         0x03F9  ; Divisor Latch High (when DLAB=1)

; Line Control bits
%define LCR_DLAB           0x80    ; Divisor Latch Access Bit
%define LCR_8BIT           0x03    ; 8 bits per character
%define LCR_NO_PARITY      0x00    ; No parity
%define LCR_ONE_STOP       0x00    ; 1 stop bit

; Line Status bits
%define LSR_THRE           0x20    ; Transmit Holding Register Empty
%define LSR_TEMT           0x40    ; Transmitter Empty
