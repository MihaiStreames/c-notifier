# c-notifier

[![License](https://img.shields.io/github/license/MihaiStreames/c-notifier?label=license)](LICENSE)

Small C binary that sends push notifications to [ntfy](https://ntfy.sh) topics from the command line using `libcurl`.

## Install

Requires `libcurl` (`curl-config` must be on `PATH`).

```sh
make
sudo make install
```

## Usage

```sh
notify -t mytopic "Build finished"
notify -t mytopic -T "Deploy" -p high -g warning "Prod deploy started"
notify -u https://ntfy.example.com/alerts "Self-hosted works too"

# no quotes needed, args are joined
notify -t mytopic build finished on branch main

# pipe the body from stdin
make 2>&1 | tail -5 | notify -t mytopic -T "Build log"

# set a default topic once, then skip -t
export NTFY_TOPIC=mytopic
notify "Done"
```

| Flag          | Meaning                                         |
| ------------- | ----------------------------------------------- |
| `-u URL`      | full endpoint URL                               |
| `-t TOPIC`    | topic on ntfy.sh                                |
| `-T TITLE`    | notification title                              |
| `-p PRIORITY` | `min`, `low`, `default`, `high`, `max` (or 1-5) |
| `-g TAGS`     | comma-separated tags/emoji shortcodes           |
| `-c URL`      | URL to open when the notification is clicked    |

Without `-u`/`-t` the endpoint comes from `NTFY_URL` or `NTFY_TOPIC`. Without a message argument the body is read from stdin.

> [!WARNING]
> Topics on the public ntfy.sh server are open to anyone who knows the name, so pick something unguessable or self-host.

## License

MIT. See [LICENSE](LICENSE).

<div align="center">
  Made with ❤️
</div>
