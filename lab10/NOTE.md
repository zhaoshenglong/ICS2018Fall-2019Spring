# Some hints

- delete your docker0
  > sudo systemctl stop docker.service
  > sudo ip link set dev docker0 down
  > sudo brctl delbr docker0

Why need to delete docker0?

```python
possible_ip = map(lambda s:s[5:], subprocess.check_output("ip addr | grep -o 'inet [^/]*'", shell=True).split("\n")[:-1])
```

and

```sh
ip addr | grep -o 'inet [^/]*'
# > contains docker0 and it must be TIMEOUT
```

- Run your client
  > stty -echo -icanon; ./server
  > NOTE: This command won't display your input. But your input will execute by bash (Actually, I don't know much about details)
