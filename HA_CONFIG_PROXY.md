# แก้ไข `configuration.yaml` ของ Home Assistant ให้เข้าผ่าน Reverse Proxy ได้

> คู่มือนี้ใช้แก้อาการ **`400: Bad Request`** ที่เกิดตอนเปิด Home Assistant ผ่านโดเมน
> (Cloudflare Tunnel → Nginx Proxy Manager) โดยการตั้ง `trusted_proxies` ให้ HA
> เชื่อใจ reverse proxy ที่ส่ง request เข้ามา

---

## ทำไมต้องแก้

Home Assistant **บล็อก reverse proxy โดย default** เพื่อความปลอดภัย เมื่อเข้าผ่าน NPM
HA จะเห็น request มาจาก IP ของ container NPM (ในเครือข่าย Docker) ซึ่งไม่ได้อยู่ใน
whitelist → HA ตอบ **400 Bad Request** ทันที

```
ผู้ใช้ ──HTTPS──▶ Cloudflare ──▶ cloudflared ──▶ NPM ──▶ homeassistant:8123
                                                  ▲ HA เห็น IP นี้ ถ้าไม่ trust → 400
```

การเพิ่ม `use_x_forwarded_for` + `trusted_proxies` คือการบอก HA ว่า "IP นี้คือ proxy
ของเราเอง เชื่อถือได้" → HA จะยอมรับ request และอ่าน IP จริงของผู้ใช้จาก header แทน

> ℹ️ ปัญหานี้เกิด **เฉพาะตอนเข้าผ่านโดเมน/proxy** — ถ้าเข้าตรงที่ `http://<VPS_IP>:8123`
> จะไม่เจอ 400 (แต่ไม่แนะนำให้เปิดพอร์ต 8123 สู่ Internet ระยะยาว)

---

## STEP 1 — หาตำแหน่งไฟล์จริงบน VPS

ไฟล์ `configuration.yaml` อยู่ใน Docker volume (`homeassistant_config`) หา path จริงด้วย:

```bash
docker volume inspect all-sys-aiots_homeassistant_config -f '{{.Mountpoint}}'
```

จะได้ path ประมาณ:

```
/var/lib/docker/volumes/all-sys-aiots_homeassistant_config/_data
```

> 💡 ถ้าคำสั่งขึ้น error `no such volume` ให้ดูชื่อ volume จริงด้วย
> `docker volume ls | grep homeassistant` แล้วแทนชื่อในคำสั่ง

---

## STEP 2 — สำรองไฟล์เดิมก่อนแก้ (กันพลาด)

```bash
CFG=$(docker volume inspect all-sys-aiots_homeassistant_config -f '{{.Mountpoint}}')/configuration.yaml
cp "$CFG" "$CFG.bak"
```

ถ้าแก้แล้วพัง ย้อนกลับได้ด้วย `cp "$CFG.bak" "$CFG"`

---

## STEP 3 — เปิดไฟล์ด้วย nano

```bash
nano "$CFG"
```

*(ตัวแปร `$CFG` มาจาก STEP 2 — ถ้าเปิด terminal ใหม่ ให้รันบรรทัด `CFG=...` ซ้ำก่อน)*

---

## STEP 4 — เพิ่มบล็อก `http:`

ในหน้าต่าง nano:

1. กด `Ctrl + W` (ค้นหา) → พิมพ์ `http:` → กด `Enter`
2. ตรวจผลลัพธ์:

### กรณี A — **ยังไม่มี** `http:` ในไฟล์

เลื่อนไปบรรทัดล่างสุด แล้ววางบล็อกนี้ทั้งก้อน:

```yaml
http:
  use_x_forwarded_for: true
  trusted_proxies:
    - 172.16.0.0/12
    - 192.168.0.0/16
    - 127.0.0.1
```

### กรณี B — **มี** `http:` อยู่แล้ว

เพิ่มเฉพาะ 2 ส่วนนี้เข้าไป**ในบล็อก `http:` เดิม** (อย่าสร้าง `http:` ใหม่ซ้ำ):

```yaml
http:
  # ...ค่าเดิมที่มีอยู่...
  use_x_forwarded_for: true
  trusted_proxies:
    - 172.16.0.0/12
    - 192.168.0.0/16
    - 127.0.0.1
```

---

## STEP 5 — บันทึกและออก

| ปุ่ม | การทำงาน |
|---|---|
| `Ctrl + O` แล้ว `Enter` | บันทึกไฟล์ |
| `Ctrl + X` | ออกจาก nano |

---

## STEP 6 — ตรวจ syntax ก่อน restart

```bash
docker exec homeassistant python -m homeassistant --script check_config -c /config
```

- ขึ้น `Testing configuration at /config` แล้วไม่มี error สีแดง = ผ่าน
- ถ้ามี error → กลับไปแก้ตาม STEP 4 (มักเป็นเรื่อง indent หรือ `http:` ซ้ำ)

---

## STEP 7 — Restart Home Assistant

```bash
docker compose restart homeassistant
```

รอ ~30 วินาที ให้ HA บูตใหม่ แล้วลองเข้า `https://ha.thaitechsync.com`
ถ้าไม่ขึ้น 400 แล้ว = **สำเร็จ** ✅

---

## กฎเหล็กเรื่อง YAML (พลาดบ่อยที่สุด)

| ✅ ถูกต้อง | ❌ ผิด |
|---|---|
| เยื้องด้วย **space** เท่านั้น | ใช้ **Tab** (YAML ห้าม Tab เด็ดขาด) |
| มี `http:` **อันเดียว** ในไฟล์ | มี `http:` ซ้ำหลายอัน |
| `use_x_forwarded_for` เยื้อง 2 ช่องใต้ `http:` | ชิดซ้าย / เยื้องไม่ตรง |
| `- 172.16.0.0/12` เยื้อง 4 ช่อง | เยื้องไม่ตรงกับรายการอื่น |

---

## Troubleshooting — ถ้ายัง 400 อยู่

### 1. ดู IP จริงที่ HA เห็น

```bash
docker compose logs homeassistant | grep -i "forwarded\|not allowed\|400"
```

มันจะบอกว่า request ถูกปฏิเสธจาก IP ไหน — เอา IP/subnet นั้นมาใส่เพิ่มใน `trusted_proxies`

### 2. หา IP จริงของ container NPM

```bash
docker inspect nginx-proxy-manager -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}'
```

ถ้า IP ที่ได้ไม่อยู่ในช่วง `172.16.0.0/12` ให้เพิ่ม subnet ของมันเข้าไปใน `trusted_proxies`

### 3. ตรวจว่า config โหลดจริง

หลัง restart แล้ว ดู log ว่ามี error ตอนบูตไหม:

```bash
docker compose logs --tail=50 homeassistant
```

---

## หมายเหตุสำคัญ

- 🧩 image นี้เป็น **HA Container** (`homeassistant/home-assistant`) — **ไม่มี Add-on store**
  จึงติดตั้ง "File editor" add-on ผ่าน GUI ไม่ได้ ต้องแก้ไฟล์จาก filesystem ตามคู่มือนี้
- 🔒 หลังเข้าผ่านโดเมนได้แล้ว ควร **ปิดพอร์ต 8123** ไม่ให้ออก Internet:
  ```bash
  ufw delete allow 8123/tcp
  ```
- 📄 คู่มือ deploy หลักดูที่ [DEPLOY_VPS.md](DEPLOY_VPS.md)
