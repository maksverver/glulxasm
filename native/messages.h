#ifndef MESSAGES_H_INCLUDED
#define MESSAGES_H_INCLUDED

/* These functions are defined as macros to avoid link-time confusion with
   (non-standard) libc functios warn() and error(). */

#define info(...) message_info(__VA_ARGS__)
#define warn(...) message_warn(__VA_ARGS__)
#define error(...) message_error(__VA_ARGS__)
#define fatal(...) message_fatal(__VA_ARGS__)

void message_info(const char *fmt, ...);
void message_warn(const char *fmt, ...);
void message_error(const char *fmt, ...);
void message_fatal(const char *fmt, ...);

#endif /* ndef MESSAGES_H_INCLUDED */
