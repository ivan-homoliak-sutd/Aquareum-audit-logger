// clang-format off

#define NOOP

#ifdef TRACING_ENABLED

    using c_charp = const char * const;

    // short file macro from https://blog.galowicz.de/2016/02/20/short_file_macro/
    static constexpr c_charp past_last_slash(c_charp str, c_charp last_slash)
    {
        return
            *str == '\0' ? last_slash :
            *str == '/'  ? past_last_slash(str + 1, str + 1) :
                        past_last_slash(str + 1, last_slash);
    }
    static constexpr c_charp past_last_slash(c_charp str)
    {
        return past_last_slash(str, str);
    }

    #define __SHORT_FILE__ ({constexpr c_charp sf__ {past_last_slash(__FILE__)}; sf__;})

    #define TRACE_ENCLAVE(fmt, ...)              \
        fprintf(stdout,                                  \
            ">\t[TRACE_ENC]: %s(%d): " fmt "\n", \
            __SHORT_FILE__,                            \
            __LINE__,                            \
            ##__VA_ARGS__)

    #define TRACE_HOST(fmt, ...)                 \
        fprintf(stdout,                                  \
            "\t[TRACE_HOST]: %s(%d): " fmt "\n", \
            __SHORT_FILE__,                            \
            __LINE__,                            \
            ##__VA_ARGS__)

    // should be define in Cmake build
    #ifdef ENCLAVE_BUILD
        #define TRACE_ME(fmt, ...) TRACE_ENCLAVE(fmt, ##__VA_ARGS__)
    #else
        #define TRACE_ME(fmt, ...)  TRACE_HOST(fmt, ##__VA_ARGS__)
    #endif

#else
    #define TRACE_ENCLAVE(fmt, ...) NOOP
    #define TRACE_HOST(fmt, ...) NOOP
    #define TRACE_ME(fmt, ...) NOOP
#endif