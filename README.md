# Nomad DB

Eventual consistent distributed key - data structure database based on CRDT

Prototype: https://github.com/realnumber3012/NOMAzurDB

Usage example:

start seed node:

```nomadb --name=seed1 --port=6666 --http_port=14780```

start common node:

```nomadb --name=node1 --port=6668 --http_port=14781```

create bucket with name "demobucket":

```curl -XPUT http://localhost:14780/storage/demobucket```

add element to bucket:

```curl -XPOST http://localhost:14780/storage/demobucket/13```

test element in the bucket:

```curl -XGET http://localhost:14780/storage/demobucket/13```

test element in the bucket from another node:

```curl -XGET http://localhost:14781/storage/demobucket/13```
