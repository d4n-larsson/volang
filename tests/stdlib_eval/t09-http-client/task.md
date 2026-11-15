A service runs on `localhost`; its port is the program's one argument. The program must:

1. `GET http://localhost:PORT/json`. The body is a JSON object. Print `name: ` followed by its `"name"` field (a string), then `tags: ` followed by the number of elements of its `"tags"` field (an array).
2. `GET http://localhost:PORT/status/404` and print `status: ` followed by the response's status code.

If a request or the JSON handling fails, print `error: ` followed by the error message and exit with status 1.

Effects: `console` and the network, limited to the host `localhost` (`net("localhost")`).
