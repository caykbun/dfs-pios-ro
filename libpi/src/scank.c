#include "rpi.h"

#define getchar rpi_getchar

inline int is_digit(int c, unsigned base) {
    switch(base) {
    case 2:
        if (c >= '0' && c <= '1')
            return c - '0';
        else
            return -1;
    case 10:
        if (c >= '0' && c <= '9')
            return c - '0';
        else
            return -1;
    case 16:
        if (c >= '0' && c <= '9')
            return c - '0';
        else if (c >= 'a' && c <= 'f')
            return (c - 'a') + 10;
        else if (c >= 'A' && c <= 'F')
            return (c - 'A') + 10;
        else
            return -1;
    default: 
        panic("invalid base=%d\n", base);
    }  
}

inline int match(int target, int *c) {
    if (*c == -1) {
        *c = getchar();
    }
    if (target == *c) {
        *c = -1;
        return 1;
    } else {
        return 0;
    }
}

static char absorb_val(unsigned base, uint32_t *u, int c_prev) {
    uint32_t result = 0;

    int c = c_prev;
    if (c_prev == -1) {
        c = getchar();
    }

    if (c != '\n') {
        while (1) {
            int digit = is_digit(c, base);
            if (digit == -1)
                break;
            switch(base) {
            case 2:
                result <<= 1;
                result += digit;
                break;
            case 10:
                result *= 10;
                result += digit;
                break;
            case 16:
                result <<= 4;
                result += digit;
                break;
            default: 
                panic("invalid base=%d\n", base);
            }    
            c = getchar();
        }
    }
    *u = result;
    return c;
}

// a really simple printk. 
// need to do <sprintk>
void vscank(const char *fmt, va_list ap) {
    int c = -1;
    for(; *fmt; fmt++) {
        // if (c == '\n') {
        //     return;
        // }
        if(*fmt != '%') {
            if (c == '\n')
                continue;
            match(*fmt, &c);
        }
        else {
            fmt++; // skip the %

            uint32_t u;
            // int v;
            char *s;
            int *ptr;
            uint32_t *ptr32;
            int flag = 0;

            switch(*fmt) {
            case 'b': c = absorb_val(2, va_arg(ap, uint32_t*), c); break;
            case 'u': c = absorb_val(10, va_arg(ap, uint32_t*), c); break;
            case 'c':
                if (c == -1) {
                    *(va_arg(ap, uint32_t*)) = getchar();
                } else {
                    *(va_arg(ap, uint32_t*)) = c;
                    c = -1;
                }
                break;

            // leading 0x
            case 'x':  
            case 'p': 
                ptr32 = va_arg(ap, uint32_t*);
                while (!match('0', &c)) {
                    if (c == '\n')
                        break;
                }
                while (!match('x', &c)) {
                    if (c == '\n')
                        break;
                }
               
                c = absorb_val(16, ptr32, c);
                break;
            // print '-' if < 0
            case 'd':
                ptr = va_arg(ap, int*);
                if (c == '\n')
                    return;
                if (match('-', &c))
                    flag = 1;
                if (c == '\n')
                    return;
                if (c == '+' || c == '-')
                    c = -1;
                c = absorb_val(10, (uint32_t *) ptr, c);
                if (flag)
                    *ptr *= -1;
                break;
            // string
            case 's':
                if (c == -1) {
                    c = getchar();
                    // printk("Wait: %c\n", c);
                }
                for(s = va_arg(ap, char *); ; s++) {
                    if (c == '\n') {
                        *s = '\0';
                        break;
                    }
                    *s = c;
                    c = getchar();
                    // printk("Wait2: %c\n", c);
                }
                break;
            default: panic("bogus identifier: <%c>\n", *fmt);
            }
        }
    }
}

int scank(const char *fmt, ...) {
    // flush it
    // while (uart_has_data())
    //     uart_get8();
    va_list args;

    int ret = 0;
    va_start(args, fmt);
       vscank(fmt, args);
    va_end(args);
    return ret;
}

