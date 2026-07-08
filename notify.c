#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct options {
    const char* url;
    const char* topic;
    const char* title;
    const char* priority;
    const char* tags;
    const char* click;
};

static void usage(FILE* stream, const char* argv0) {
    fprintf(stream, "Usage: %s [-u URL | -t TOPIC] [OPTIONS] [MESSAGE...]\n", argv0);
    fprintf(stream, "  -u URL       full endpoint URL\n");
    fprintf(stream, "  -t TOPIC     topic on ntfy.sh (shortcut for -u https://ntfy.sh/TOPIC)\n");
    fprintf(stream, "  -T TITLE     notification title\n");
    fprintf(stream, "  -p PRIORITY  min, low, default, high, max (or 1-5)\n");
    fprintf(stream, "  -g TAGS      comma-separated tags\n");
    fprintf(stream, "  -c URL       URL to open when the notification is clicked\n");
    fprintf(stream, "  -h           show help\n");
    fprintf(stream, "\n");
    fprintf(stream, "Without -u/-t the endpoint comes from NTFY_URL or NTFY_TOPIC.\n");
    fprintf(stream, "Without MESSAGE the body is read from stdin.\n");
}

static void parse_options(int argc, char* argv[], struct options* opts) {
    int opt;
    while ((opt = getopt(argc, argv, "u:t:T:p:g:c:h")) != -1) {
        switch (opt) {
            case 'u':
                opts->url = optarg;
                break;
            case 't':
                opts->topic = optarg;
                break;
            case 'T':
                opts->title = optarg;
                break;
            case 'p':
                opts->priority = optarg;
                break;
            case 'g':
                opts->tags = optarg;
                break;
            case 'c':
                opts->click = optarg;
                break;
            case 'h':
                usage(stdout, argv[0]);
                exit(0);
            default:
                usage(stderr, argv[0]);
                exit(2);
        }
    }
}

// precedence: -u, -t, NTFY_URL, NTFY_TOPIC; buf backs the topic-built URLs
static const char* resolve_url(const struct options* opts, char* buf, size_t bufsize) {
    const char* env_url   = getenv("NTFY_URL");
    const char* env_topic = getenv("NTFY_TOPIC");

    if (opts->url) {
        return opts->url;
    }

    if (opts->topic) {
        snprintf(buf, bufsize, "https://ntfy.sh/%s", opts->topic);
        return buf;
    }

    if (env_url && *env_url) {
        return env_url;
    }

    if (env_topic && *env_topic) {
        snprintf(buf, bufsize, "https://ntfy.sh/%s", env_topic);
        return buf;
    }

    fprintf(stderr, "error: no endpoint\n");
    exit(2);
}

static char* join_args(int argc, char* argv[], int start) {
    size_t len = 0;
    char*  msg;

    for (int i = start; i < argc; i++) {
        len += strlen(argv[i]) + 1;
    }

    msg = malloc(len);
    if (!msg) {
        return NULL;
    }

    msg[0] = '\0';
    for (int i = start; i < argc; i++) {
        strcat(msg, argv[i]);
        if (i + 1 < argc) {
            strcat(msg, " ");
        }
    }

    return msg;
}

static char* read_stdin(void) {
    size_t cap = 4096;
    size_t len = 0;
    char*  buf = malloc(cap);

    if (!buf) {
        return NULL;
    }

    for (;;) {
        size_t n = fread(buf + len, 1, cap - len - 1, stdin);
        len += n;
        if (n == 0) {
            break;
        }

        if (len + 1 == cap) {
            char* grown = realloc(buf, cap *= 2);
            if (!grown) {
                free(buf);
                return NULL;
            }

            buf = grown;
        }
    }

    buf[len] = '\0';
    return buf;
}

// message from remaining args, or stdin when piped; strips trailing newlines
static char* get_message(int argc, char* argv[]) {
    char*  message;
    size_t len;

    if (optind < argc) {
        message = join_args(argc, argv, optind);
    } else if (!isatty(STDIN_FILENO)) {
        message = read_stdin();
    } else {
        usage(stderr, argv[0]);
        exit(2);
    }

    if (!message) {
        fprintf(stderr, "error: out of memory\n");
        exit(1);
    }

    len = strlen(message);
    while (len > 0 && (message[len - 1] == '\n' || message[len - 1] == '\r')) {
        message[--len] = '\0';
    }

    if (len == 0) {
        fprintf(stderr, "error: empty message\n");
        exit(2);
    }

    return message;
}

static struct curl_slist* add_header(struct curl_slist* list, const char* name, const char* value) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s: %s", name, value);
    return curl_slist_append(list, buf);
}

static struct curl_slist* build_headers(const struct options* opts) {
    struct curl_slist* headers = NULL;

    if (opts->title) {
        headers = add_header(headers, "Title", opts->title);
    }

    if (opts->priority) {
        headers = add_header(headers, "Priority", opts->priority);
    }

    if (opts->tags) {
        headers = add_header(headers, "Tags", opts->tags);
    }

    if (opts->click) {
        headers = add_header(headers, "Click", opts->click);
    }

    return headers;
}

// swallow the response body; ntfy echoes the published message as JSON on stdout otherwise
static size_t discard(void* data, size_t size, size_t nmemb, void* userdata) {
    (void)data;
    (void)userdata;
    return size * nmemb;
}

static int send_notification(const char* url, const char* message, struct curl_slist* headers) {
    CURL*    curl;
    CURLcode result;

    char errbuf[CURL_ERROR_SIZE] = "";
    long code                    = 0;

    int exit_code = 0;

    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "error: curl_easy_init failed\n");
        return 1;
    }

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);

    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");

    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, message);
    if (headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        fprintf(stderr, "error: %s\n", errbuf[0] ? errbuf : curl_easy_strerror(result));
        exit_code = 1;
    } else {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        if (code >= 400) {
            fprintf(stderr, "error: server returned HTTP %ld\n", code);
            exit_code = 1;
        }
    }

    curl_easy_cleanup(curl);
    return exit_code;
}

int main(int argc, char* argv[]) {
    struct options opts = {0};
    char           urlbuf[512];

    const char* url;
    char*       message;

    struct curl_slist* headers;

    int exit_code;

    parse_options(argc, argv, &opts);
    url     = resolve_url(&opts, urlbuf, sizeof(urlbuf));
    message = get_message(argc, argv);

    if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
        fprintf(stderr, "error: curl_global_init failed\n");
        free(message);
        return 1;
    }

    headers   = build_headers(&opts);
    exit_code = send_notification(url, message, headers);

    curl_slist_free_all(headers);
    free(message);
    curl_global_cleanup();
    return exit_code;
}
