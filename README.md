ReBus
===============
Representaional Bus

Summary
------------
ReBus is an IPC stack for Laniakea desktop. Inspired by REST API architecture.

Data Format
------------
The payload and response format are available JSON types.

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
Get hosts or post a new host.
| Method | Response       |
| ------ | -------------- |
| GET    | List of hosts  |
| POST   | ?              |

### /rebus/kill

