# wake-service

Starts the Drone Challenger streaming instance (EC2) when a player opens `dronechallenger.stamsite.cc`, shows a waking page while it boots, and proxies the Pixel Streaming player page and signalling websocket to it once the game is streamable. Runs on the shared Lightsail box next to `refinery.stamsite.cc`. Design: [context/wake-service.md](context/wake-service.md).

The host IP, instance ID and keys are not in this repo. The commands below use `$LIGHTSAIL_HOST` and `$LIGHTSAIL_KEY`.

## Deploy

```
scp -i "$LIGHTSAIL_KEY" -r wake-service ubuntu@"$LIGHTSAIL_HOST":~/dronechallenger-wake-service
ssh -i "$LIGHTSAIL_KEY" ubuntu@"$LIGHTSAIL_HOST"
cd ~/dronechallenger-wake-service && cp .env.example .env   # fill in the IAM user's keys and EC2_INSTANCE_ID
docker compose up -d --build
curl http://127.0.0.1:8091/_wake/status
sudo cp nginx-site.conf /etc/nginx/sites-available/dronechallenger.stamsite.cc
sudo ln -sf /etc/nginx/sites-available/dronechallenger.stamsite.cc /etc/nginx/sites-enabled/dronechallenger.stamsite.cc
sudo nginx -t && sudo systemctl reload nginx
```

DNS: a Cloudflare-proxied A record `dronechallenger` in `stamsite.cc` pointing at the Lightsail box.

The IAM user needs `ec2:DescribeInstances` and `ec2:StartInstances` (plus `ec2:StopInstances` if `IDLE_STOP_MINUTES` is enabled) on the instance.

## Redeploy after a code change

```
scp -i "$LIGHTSAIL_KEY" main.py ubuntu@"$LIGHTSAIL_HOST":~/dronechallenger-wake-service/main.py
ssh -i "$LIGHTSAIL_KEY" ubuntu@"$LIGHTSAIL_HOST" "cd ~/dronechallenger-wake-service && docker compose up -d --build"
```

## Logs

Wake, ready and stop events: `~/dronechallenger-wake-service/logs/wake-events.jsonl` on the Lightsail box, or `docker compose logs wake`.
