ReBus
===============
Representaional Bus

Summary
------------
ReBus is an IPC stack for Laniakea desktop. Inspired by REST API architecture.

Data Format
------------
The payload and response format are available JSON types.

Test
------------
You can test APIs by curl with `--unix-socket` option.

Example
```sh
$ curl -v --unix-socket $XDG_RUNTIME_DIR/rebus.sock -XGET http://rebus/ping
*   Trying /run/user/1000/rebus.sock:0...
* Connected to rebus (/run/user/1000/rebus.sock) port 80 (#0)
> GET /ping HTTP/1.1
> Host: rebus
> User-Agent: curl/7.71.1
> Accept: */*
>
* Mark bundle as not supporting multiuse
< HTTP/1.1 200 OK
< Content-Type: application/json
< Server: ReBus 0.1
< Content-Length: 6
<
* Connection #0 to host rebus left intact
"pong"
```

ReBus APIs
------------

### /rebus/ping
Simple ping API.
| Method | Response |
| ------ | -------- |
| GET    | `"pong"` |

### /rebus/version
Get version of ReBus.
| Method | Response       |
| ------ | -------------- |
| GET    | Version string |

### /rebus/hosts
Get hosts, post a new host or delete a host.
| Method | Response       |
| ------ | -------------- |
| GET    | List of hosts  |
| POST   | ?              |
| DELETE | ?              |

### /rebus/kill
Kill the running ReBus process.
| Method | Response | Status |
| ------ | -------- | ------ |
| POST   | No       | 202    |
