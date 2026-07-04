# 🚀 คู่มือ Deploy ระบบบน VPS (Production)

> คู่มือ step-by-step สำหรับนำ IoT Stack (Node-RED + N8N + InfluxDB + Grafana + Home Assistant)
> ขึ้นรันบน VPS จริง พร้อม security hardening และเปิดใช้งานผ่าน **Cloudflare Tunnel**
>
> 📖 ก่อนเริ่ม แนะนำอ่าน [README.md](README.md) ให้เข้าใจโครงสร้างระบบก่อน

---

## 🎯 ภาพรวมสถาปัตยกรรม Production

```
   Internet (ผู้ใช้)
        │  HTTPS
        ▼
  ┌──────────────────┐
  │  Cloudflare Edge │  (SSL, DDoS protection, DNS)
  └────────┬─────────┘
           │  Tunnel (outbound-only, ไม่ต้องเปิด inbound port)
           ▼
  ┌─────────────────────── VPS ───────────────────────┐
  │  cloudflared ──▶ nginx-proxy-manager ──▶ services  │
  │                                                    │
  │  Admin UIs (1880/3000/8086/81...) ผูกกับ 127.0.0.1 │
  │  → เข้าถึงผ่าน SSH tunnel เท่านั้น (ไม่เปิด public) │
  └────────────────────────────────────────────────────┘
```

**หลักการความปลอดภัย:**
- 🔒 **ไม่เปิดพอร์ต inbound** สู่ Internet เลย (ยกเว้น SSH) — Cloudflare Tunnel เชื่อมแบบ outbound
- 🔒 Admin UI ทั้งหมดผูกกับ `127.0.0.1` → เข้าถึงผ่าน **SSH port-forward** เท่านั้น
- 🔒 Public traffic เข้าได้เฉพาะ service ที่ตั้งใจเปิดผ่าน Cloudflare + NPM

---

## ✅ สิ่งที่ต้องเตรียม

| รายการ | รายละเอียด |
|---|---|
| **VPS** | Ubuntu 22.04 / 24.04 LTS · อย่างน้อย **2 vCPU / 4 GB RAM / 40 GB SSD** (Home Assistant + 8 containers กินทรัพยากรพอควร) |
| **โดเมน** | โดเมนที่ชี้ nameserver มายัง Cloudflare แล้ว |
| **บัญชี Cloudflare** | ฟรีก็พอ (สำหรับ Tunnel) |
| **SSH key** | สร้างไว้บนเครื่องตัวเอง (`ssh-keygen -t ed25519`) |

---

## STEP 1 — เชื่อมต่อ VPS ครั้งแรก & อัปเดตระบบ

```bash
# จากเครื่องตัวเอง
ssh root@<VPS_IP>
```

```bash
# บน VPS
apt update && apt upgrade -y
apt install -y ufw fail2ban git curl
timedatectl set-timezone Asia/Bangkok
```

---

## STEP 2 — สร้าง User ใหม่ & ปิด root login (Hardening)

```bash
# สร้าง user (แทน deploy ด้วยชื่อที่ต้องการ)
adduser deploy
usermod -aG sudo deploy

# คัดลอก SSH key จาก root ไปให้ user ใหม่
rsync --archive --chown=deploy:deploy ~/.ssh /home/deploy
```

> จากนั้น **ทดสอบ login ด้วย user ใหม่** ในหน้าต่าง terminal ใหม่ ก่อนปิด root:
> ```bash
> ssh deploy@<VPS_IP>
> ```

เมื่อ login ด้วย user ใหม่ได้แล้ว → แก้ SSH config ปิด root/password login:

```bash
sudo nano /etc/ssh/sshd_config
```
ตั้งค่า:
```
PermitRootLogin no
PasswordAuthentication no
```
```bash
sudo systemctl restart ssh
```

---

## STEP 3 — ตั้งค่า Firewall (UFW)

> เพราะใช้ **Cloudflare Tunnel** (เชื่อม outbound) จึง **ไม่ต้องเปิดพอร์ต 80/443** สู่ Internet
> เปิดแค่ SSH พอ

```bash
sudo ufw default deny incoming
sudo ufw default allow outgoing
sudo ufw allow OpenSSH          # หรือพอร์ต SSH ที่กำหนดเอง
sudo ufw enable
sudo ufw status verbose
```

> 📶 **หมายเหตุ MQTT:** ถ้าอุปกรณ์ ESP32 อยู่นอกวง VPS และต้องต่อ MQTT ตรง (port 1883)
> ควร (ก) เปิด auth ใน Mosquitto ก่อน แล้วค่อย `sudo ufw allow 1883/tcp` หรือ
> (ข) ให้อุปกรณ์เชื่อมผ่าน VPN/Cloudflare แทน — **อย่าเปิด 1883 แบบ anonymous สู่ Internet**

---

## STEP 4 — ติดตั้ง Docker & Docker Compose

```bash
# ติดตั้ง Docker Engine (official script)
curl -fsSL https://get.docker.com | sudo sh

# ให้ user รัน docker ได้โดยไม่ต้อง sudo
sudo usermod -aG docker deploy
newgrp docker    # หรือ logout/login ใหม่

# ตรวจสอบ
docker --version
docker compose version
```

---

## STEP 5 — Clone โปรเจกต์ & สร้าง `.env`

```bash
cd ~
git clone <repo-url> iot-stack
cd iot-stack
cp .env.example .env
```

สร้าง credentials ทั้งหมด (ทำตาม [SETUP_CREDENTIALS.md](SETUP_CREDENTIALS.md)) แล้วแก้ `.env`:

```bash
# สร้าง Node-RED password hash (จำ: ต้อง escape $ เป็น $$ ใน .env!)
npx bcryptjs "รหัสผ่านที่แข็งแรง" 8

# N8N encryption key
openssl rand -hex 16

# InfluxDB admin token
openssl rand -hex 32
```

```bash
nano .env
```

> ### ⚠️ Checklist ค่าใน `.env` สำหรับ Production
> - [ ] `NODERED_PASSWORD_HASH` — escape `$` → `$$` ทุกตัว
> - [ ] `INFLUXDB_PASSWORD` / `GRAFANA_ADMIN_PASSWORD` — **รหัสแข็งแรง ไม่ซ้ำกัน** (ห้ามใช้ `admin1234`)
> - [ ] `INFLUXDB_ADMIN_TOKEN` / `N8N_ENCRYPTION_KEY` — สุ่มใหม่จริง
> - [ ] `N8N_DOMAIN` — โดเมนจริง เช่น `n8n.yourdomain.com`
> - [ ] `CLOUDFLARE_TUNNEL_TOKEN` — ได้จาก Step 7
> - [ ] `TZ=Asia/Bangkok`

---

## STEP 5B — เครื่องมือจัดการค่า `.env` ใน Production

`.env` มี secret ทั้งหมด (**ห้าม commit ขึ้น git** — อยู่ใน `.gitignore` แล้ว) เลือกวิธีจัดการตามระดับทีม/ความปลอดภัยที่ต้องการ:

| วิธี | เครื่องมือ | เก็บใน git ได้? | เหมาะกับ |
|---|---|---|---|
| **A** แก้บน VPS ตรง ๆ | `nano` / `vim` | ❌ | เริ่มต้น / solo |
| **B** สร้างที่เครื่องแล้วส่งขึ้น | `scp` / `rsync` (ผ่าน SSH) | ❌ | solo / ทีมเล็ก |
| **C** เข้ารหัสเก็บใน git | **SOPS + age** | ✅ (เข้ารหัส) | ทีม + ต้องการ version control |
| **D** Secrets Manager | **Infisical / Doppler / Vault** | ❌ (อยู่ที่ service) | หลาย environment / ทีมใหญ่ |

---

### วิธี A — แก้บน VPS โดยตรง (ง่ายสุด)

```bash
cd ~/iot-stack
nano .env            # ใส่ค่าทั้งหมด
chmod 600 .env       # ให้เจ้าของอ่าน/เขียนเท่านั้น (สำคัญ)
ls -l .env           # ต้องเห็น -rw------- (600)
```

### วิธี B — สร้างที่เครื่องตัวเองแล้ว copy ขึ้นแบบปลอดภัย

เตรียม `.env` บนเครื่องตัวเอง (มีเครื่องมือสร้าง hash/token ครบ) แล้วส่งผ่าน SSH:

```bash
# บนเครื่องตัวเอง — ส่งขึ้น VPS ผ่านช่องเข้ารหัส SSH
scp .env deploy@<VPS_IP>:~/iot-stack/.env

# แล้ว set สิทธิ์บน VPS
ssh deploy@<VPS_IP> "chmod 600 ~/iot-stack/.env"
```

> ✅ `scp`/`rsync` วิ่งผ่าน SSH (เข้ารหัส) จึงปลอดภัย · ❌ อย่าส่ง `.env` ผ่าน chat/email/git

### วิธี C — SOPS + age (เข้ารหัส `.env` เก็บใน git ได้)

เหมาะเมื่อต้องการเก็บ config เข้ารหัสไว้ใน repo (version control) แต่ไม่เปิดเผย secret

```bash
# 1. ติดตั้ง (บนเครื่องตัวเอง + VPS)
#    - sops: https://github.com/getsops/sops/releases
#    - age:  https://github.com/FiloSottile/age
sudo apt install -y age
# ติดตั้ง sops จาก release (ตัวอย่าง Linux amd64)
curl -Lo sops https://github.com/getsops/sops/releases/latest/download/sops-v3.9.0.linux.amd64
chmod +x sops && sudo mv sops /usr/local/bin/

# 2. สร้าง key (เก็บ key ไฟล์นี้ให้ดี — ใช้ถอดรหัส)
age-keygen -o key.txt
#   → บันทึก public key ที่ขึ้นต้น age1...

# 3. สร้าง .sops.yaml (ในโปรเจกต์) บอกให้เข้ารหัสด้วย public key
cat > .sops.yaml <<'EOF'
creation_rules:
  - path_regex: \.env$
    age: "age1xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"   # public key ของคุณ
EOF

# 4. เข้ารหัส .env → commit ไฟล์ .enc ขึ้น git ได้อย่างปลอดภัย
sops --encrypt .env > .env.enc
git add .env.enc .sops.yaml          # .env.enc เข้ารหัสแล้ว commit ได้

# 5. ตอน deploy บน VPS — ถอดรหัสกลับเป็น .env
export SOPS_AGE_KEY_FILE=~/key.txt   # ต้องมี key.txt บน VPS (ส่งผ่าน scp)
sops --decrypt .env.enc > .env
chmod 600 .env
```

> 🔑 **key.txt คือกุญแจถอดรหัส** — เก็บนอก git, สำรองไว้ที่ปลอดภัย (password manager) ถ้าหายจะถอดรหัสไม่ได้

### วิธี D — Secrets Manager (Infisical / Doppler / Vault)

Inject ค่า env ตอนรัน โดยไม่ต้องมีไฟล์ `.env` บนเครื่องเลย เหมาะกับหลาย environment (dev/staging/prod)

```bash
# ── Doppler ──
doppler login
doppler setup                        # เลือก project + config (prod)
doppler run -- docker compose up -d   # inject env เข้า compose ตอนรัน

# ── Infisical (open-source, self-host ได้) ──
infisical login
infisical run --env=prod -- docker compose up -d
```

> ข้อดี: หมุนเวียน (rotate) secret ได้จากส่วนกลาง, มี audit log, แยกสิทธิ์ตามคน
> ข้อเสีย: ต้อง setup service เพิ่ม + ผูกกับ vendor

---

### 📌 คำแนะนำ

| สถานการณ์ | ใช้วิธี |
|---|---|
| Deploy คนเดียว ครั้งเดียว | **A** หรือ **B** |
| ทีมเล็ก อยากเก็บ config ใน git | **C** (SOPS + age) |
| หลาย environment / ทีมใหญ่ / ต้อง audit | **D** (Infisical/Doppler/Vault) |

> ไม่ว่าวิธีใด กฎเหล็ก: **`.env` ต้อง `chmod 600` เสมอ · ไม่ commit `.env` ดิบขึ้น git · สำรอง secret ไว้ที่ปลอดภัย**

---

## STEP 6 — ปิดพอร์ต Admin ไม่ให้ออก Public (สำคัญมาก)

โดย default `docker-compose.yml` ผูกพอร์ตกับ `0.0.0.0` (ทุก interface) ซึ่งบน VPS
สาธารณะจะ **เสี่ยงมาก** ให้สร้างไฟล์ **override** เพื่อผูกพอร์ต admin กับ `127.0.0.1` เท่านั้น

สร้างไฟล์ `docker-compose.override.yml`:

```yaml
# docker-compose.override.yml — Production port hardening
# ผูกพอร์ต admin กับ localhost เท่านั้น (เข้าถึงผ่าน SSH tunnel)
services:
  nodered:
    ports: !override
      - "127.0.0.1:1880:1880"
  influxdb:
    ports: !override
      - "127.0.0.1:8086:8086"
  n8n:
    ports: !override
      - "127.0.0.1:5678:5678"
  grafana:
    ports: !override
      - "127.0.0.1:3000:3000"
  homeassistant:
    ports: !override
      - "127.0.0.1:8123:8123"
  nginx-proxy-manager:
    ports: !override
      - "127.0.0.1:81:81"      # admin UI → localhost เท่านั้น
      - "127.0.0.1:80:80"      # cloudflared เชื่อมผ่าน docker network อยู่แล้ว
      - "127.0.0.1:443:443"
  mosquitto:
    ports: !override
      - "127.0.0.1:1883:1883"  # เปิด public เฉพาะเมื่อเปิด auth แล้ว
      - "127.0.0.1:9001:9001"
```

> Docker Compose จะรวม `docker-compose.override.yml` เข้ากับไฟล์หลักอัตโนมัติ
> `!override` แทนที่ ports เดิมทั้งหมด (ไม่ใช่ต่อท้าย)

---

## STEP 7 — สร้าง Cloudflare Tunnel

1. เข้า https://one.dash.cloudflare.com → **Networks → Tunnels → Create a tunnel**
2. เลือกชนิด **Cloudflared** → ตั้งชื่อ → **Save**
3. หน้า install เลือก **Docker** → **copy token** (ขึ้นต้น `eyJ...`)
4. ใส่ token ลง `.env`:
   ```env
   CLOUDFLARE_TUNNEL_TOKEN=eyJhIjoi...
   ```
5. เพิ่ม **Public Hostname** (แต่ละ service ที่ต้องการเปิด public):

   | Subdomain | Path | Service (Type: HTTP) |
   |---|---|---|
   | `n8n.yourdomain.com` | | `nginx-proxy-manager:80` |
   | `nodered.yourdomain.com` | | `nginx-proxy-manager:80` |
   | `grafana.yourdomain.com` | | `nginx-proxy-manager:80` |

   > ให้ทุก hostname ชี้ไปที่ `nginx-proxy-manager:80` แล้วให้ NPM แยก routing ต่อ (Step 9)

---

## STEP 8 — เปิดระบบขึ้น

```bash
cd ~/iot-stack
docker compose up -d
docker compose ps          # รอจน healthy ทุกตัว
docker compose logs -f cloudflared   # ตรวจว่า tunnel เชื่อมสำเร็จ
```

> ✅ InfluxDB จะ init org/bucket/token จาก `.env` อัตโนมัติ (volume ใหม่บน VPS = สะอาด)
> ตรวจ: `docker exec influxdb influx org list`

---

## STEP 9 — ตั้งค่า Nginx Proxy Manager

เข้า NPM ผ่าน **SSH tunnel** (ปลอดภัย ไม่เปิด public):

```bash
# บนเครื่องตัวเอง
ssh -L 8181:localhost:81 deploy@<VPS_IP>
```
เปิดเบราว์เซอร์ → http://localhost:8181

1. Login ครั้งแรก: `admin@example.com` / `changeme` → **เปลี่ยนอีเมล+รหัสทันที**
2. **Hosts → Proxy Hosts → Add Proxy Host** (ทำทีละ service):

   | ช่อง | ค่า (ตัวอย่าง n8n) |
   |---|---|
   | Domain Names | `n8n.yourdomain.com` |
   | Scheme | `http` |
   | Forward Hostname | `n8n` (ชื่อ container) |
   | Forward Port | `5678` |
   | Websockets Support | ✅ เปิด (จำเป็นสำหรับ n8n/Node-RED) |

   ทำซ้ำสำหรับ: `nodered → 1880`, `grafana → 3000`

3. **SSL:** เพราะ Cloudflare จัดการ SSL ที่ edge อยู่แล้ว ตั้ง Cloudflare SSL mode เป็น **Full**
   (ถ้าต้องการ cert ที่ NPM ด้วย ใช้ Let's Encrypt ในแท็บ SSL — แต่ต้องเปิด 80/443 public)

---

## STEP 10 — เข้าใช้งาน & ตั้งค่าครั้งแรก

**เข้าผ่าน public (Cloudflare):**
- https://n8n.yourdomain.com → สร้างบัญชี owner
- https://nodered.yourdomain.com → login `admin` + รหัส plain
- https://grafana.yourdomain.com → login `GRAFANA_ADMIN_USER` / password

**เข้า admin ภายใน (ผ่าน SSH tunnel):**
```bash
ssh -L 8086:localhost:8086 -L 3000:localhost:3000 deploy@<VPS_IP>
```
- http://localhost:8086 (InfluxDB) · http://localhost:3000 (Grafana)

**Grafana → เพิ่ม InfluxDB data source:**
- URL `http://influxdb:8086` · Query language **Flux**
- Org/Token/Bucket = ค่าจาก `.env`

---

## 🔧 การดูแลรักษา (Maintenance)

### Backup (สำคัญที่สุด)

ข้อมูลอยู่ใน Docker volumes ทั้งหมด — สำรองด้วยการ dump volume:

```bash
# สคริปต์ backup ง่าย ๆ — เก็บ volume สำคัญเป็น .tar.gz
mkdir -p ~/backups && cd ~/backups
for v in nodered_data n8n_data influxdb_data grafana_data homeassistant_config npm_data; do
  docker run --rm -v iot-stack_$v:/data -v $(pwd):/backup alpine \
    tar czf /backup/${v}_$(date +%F).tar.gz -C /data .
done
```
> ⚠️ อย่าลืม backup ไฟล์ **`.env`** แยกไว้ที่ปลอดภัย (มี secret ทั้งหมด)
> 💡 ตั้ง cron ให้ backup อัตโนมัติ + ส่งขึ้น object storage (S3/R2)

### อัปเดต image

```bash
cd ~/iot-stack
docker compose pull
docker compose up -d
docker image prune -f      # ลบ image เก่าที่ไม่ใช้
```

### ดู log / สถานะ

```bash
docker compose ps
docker compose logs -f --tail=100 nodered
docker stats               # ดูการใช้ CPU/RAM แต่ละ container
```

### แก้ `.env` แล้วโหลดใหม่

```bash
docker compose up -d <service>     # ไม่ใช่ restart! (restart ไม่โหลด env ใหม่)
```

### รีสตาร์ททั้งระบบหลัง reboot VPS

`restart: unless-stopped` ใน compose ทำให้ container ขึ้นเองหลัง reboot อยู่แล้ว
ถ้าต้องการสั่งเอง: `docker compose up -d`

---

## 🐛 Troubleshooting

| อาการ | วิธีแก้ |
|---|---|
| Cloudflare Tunnel ไม่เชื่อม | `docker compose logs cloudflared` — ตรวจ token ถูกต้อง, VPS ออก internet ได้ |
| เปิดโดเมนแล้ว 502 Bad Gateway | ตรวจ NPM Proxy Host: Forward Hostname/Port ตรง service, เปิด Websockets |
| Node-RED login ไม่ได้ | `docker exec nodered printenv NODERED_PASSWORD_HASH` ต้อง 60 ตัว — ถ้าสั้นแปลว่าไม่ได้ escape `$$` |
| RAM เต็ม / container ถูก kill | เพิ่ม RAM หรือปิด service ที่ไม่ใช้ (เช่น homeassistant) + เพิ่ม swap |
| InfluxDB org ไม่ตรง `.env` | volume ถูก init ด้วยค่าเก่า — ดู [README.md](README.md) Step 5 (ล้าง volume influxdb) |
| เข้า admin UI จากภายนอกไม่ได้ | ถูกต้องแล้ว! (ผูก 127.0.0.1) — ใช้ SSH tunnel: `ssh -L <port>:localhost:<port> deploy@VPS` |

---

## 🔐 Security Checklist ก่อนขึ้น Production

- [ ] ปิด root login + password auth (SSH key เท่านั้น)
- [ ] UFW เปิดเฉพาะ SSH
- [ ] เปลี่ยนรหัส default ทุกตัว (NPM `admin@example.com/changeme`, Grafana, InfluxDB)
- [ ] Admin ports ผูก `127.0.0.1` (ผ่าน `docker-compose.override.yml`)
- [ ] Mosquitto — เปิด auth หรือไม่ expose 1883 สู่ public
- [ ] `.env` สิทธิ์ไฟล์ `chmod 600 .env` และไม่ commit ขึ้น git
- [ ] ตั้ง backup อัตโนมัติ + ทดสอบ restore
- [ ] เปิด Cloudflare Access (Zero Trust) หน้า admin subdomain ถ้าต้องการชั้นป้องกันเพิ่ม

---

## 🔗 เอกสารที่เกี่ยวข้อง

- [README.md](README.md) — โครงสร้างระบบ & การติดตั้ง local
- [SETUP_CREDENTIALS.md](SETUP_CREDENTIALS.md) — สร้าง hash / key / token
- [esp32-firmware-NodeRED/README.md](esp32-firmware-NodeRED/README.md) — firmware ฝั่งอุปกรณ์
