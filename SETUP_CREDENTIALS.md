# Setup Credentials - Command Line Guide

> คู่มือการสร้าง Password Hash และ Tokens แบบ Command Line

---

## 📋 Table of Contents

1. [Node-RED Password Hash](#node-red-password-hash)
2. [N8N Encryption Key](#n8n-encryption-key)
3. [InfluxDB Admin Token](#influxdb-admin-token)
4. [สรุป & Next Steps](#สรุป--next-steps)

---

## 🔐 Node-RED Password Hash

### ต้องทำ: สร้าง bcrypt password hash

### Step 1: สร้าง Password Hash (คำสั่งเดียว)

พิมพ์รหัสผ่านตรง ๆ ในคำสั่งเดียว → ได้ hash ทันที (ไม่ต้องติดตั้งอะไรล่วงหน้า `npx` จะดึง `bcryptjs` ให้เอง):

**Windows / Mac / Linux (เหมือนกัน):**
```bash
npx bcryptjs "รหัสผ่านที่ต้องการ" 8
```

> เลข `8` ท้ายสุดคือจำนวน bcrypt rounds (ค่ามาตรฐานของ Node-RED)

**ทางเลือกอื่น:**
- แบบโต้ตอบ: `npm install -g node-red-admin` แล้ว `node-red-admin hash-pw` (พิมพ์รหัส 2 ครั้ง)
- Online: https://bcrypt.online/ (ใส่ password, ตั้ง Rounds = 8, กด Hash)

### Step 2: Copy Hash ที่ได้

**Output ตัวอย่าง:**
```
$2b$08$e9VVIc9vJpnsw9LotxfoeOB.EfPAxbZ39HDts2DEbY38Gm5M2r7X2
```

### Step 3: 🔴 escape `$` เป็น `$$` แล้วใส่ลง .env

> ### ⚠️⚠️ จุดพลาดบ่อยที่สุด — ต้อง escape ก่อนวาง!
>
> Docker Compose ตีความ `$` เป็นตัวแปร ถ้าวาง hash ดิบ ๆ → hash ขาด → **login ไม่ได้**
> ให้เปลี่ยน `$` **ทุกตัว** เป็น `$$`

| | ค่า |
|---|---|
| hash ที่ generator ให้มา | `$2b$08$e9VV...` |
| ที่ต้องวางใน `.env` | `$$2b$$08$$e9VV...` |

**ใส่ลงใน .env:**
```env
NODERED_PASSWORD_HASH=$$2b$$08$$e9VVIc9vJpnsw9LotxfoeOB.EfPAxbZ39HDts2DEbY38Gm5M2r7X2
```

### ⚠️ ตรวจสอบ Hash

```
✓ ต้องเริ่มด้วย: $2a$ หรือ $2b$  (ก่อน escape)
✓ ต้องยาว: 60 ตัวอักษร  (นับแบบ $ เดี่ยว)
✓ ใน .env ต้อง escape เป็น $$ ทุกตัว
✓ ไม่มี: space หรือ newline
```

**ยืนยันว่า container ได้รับ hash ถูกต้อง** (หลัง `docker compose up -d`):
```bash
docker exec nodered printenv NODERED_PASSWORD_HASH
# ต้องได้ hash เต็ม 60 ตัว ($ เดี่ยว) — ถ้าสั้นกว่านั้นแปลว่ายังไม่ได้ escape $$
```

---

## 🔑 N8N Encryption Key

### ต้องทำ: สร้าง 32-character hex string

### วิธีที่ 1: Node.js (ทุก platform — แนะนำ)

```bash
node -e "console.log(require('crypto').randomBytes(16).toString('hex'))"
```

### Windows PowerShell

**ใช้ PowerShell command:**

```powershell
[System.Guid]::NewGuid().ToString().Replace("-","").Substring(0,32)
```

**Step-by-step:**
```powershell
# 1. เปิด PowerShell
Windows Key → พิมพ์ "PowerShell" → Enter

# 2. Copy command ด้านบน แล้ว Paste ใน PowerShell
[System.Guid]::NewGuid().ToString().Replace("-","").Substring(0,32)

# 3. Press Enter

# Output ควรจะเป็น:
# 8f3a2b1c9d7e5f4a6b2c1d9e8f7a3b2c
```

**ใส่ลงใน .env:**
```env
N8N_ENCRYPTION_KEY=8f3a2b1c9d7e5f4a6b2c1d9e8f7a3b2c
```

### Mac/Linux Terminal

**วิธีที่ 1: ใช้ openssl (เร็วสุด)**

```bash
openssl rand -hex 16
```

**Step-by-step:**
```bash
# 1. เปิด Terminal
Cmd+Space → พิมพ์ "Terminal" → Enter

# 2. Copy command ด้านบน แล้ว Paste ใน Terminal
openssl rand -hex 16

# 3. Press Enter

# Output ควรจะเป็น:
# 8f3a2b1c9d7e5f4a6b2c1d9e8f7a3b2c
```

**ใส่ลงใน .env:**
```env
N8N_ENCRYPTION_KEY=8f3a2b1c9d7e5f4a6b2c1d9e8f7a3b2c
```

### ⚠️ ตรวจสอบ Key

```
✓ ต้องยาว: 32 ตัวอักษร
✓ ต้องเป็น: hex (0-9, a-f)
✓ ไม่มี: dash (-) หรือเครื่องหมายพิเศษ
```

---

## 🎫 InfluxDB Admin Token

> ✅ ค่า `INFLUXDB_ORG` / `INFLUXDB_BUCKET` / `INFLUXDB_ADMIN_TOKEN` ที่ตั้งใน `.env`
> จะถูกใช้ init InfluxDB **อัตโนมัติ** ตอน `docker compose up -d` (ไม่ต้องสร้างใน UI)
> — แค่สร้าง token ตามด้านล่างแล้วใส่ `.env` ก็พอ
>
> 📝 หมายเหตุ: **ORG / BUCKET เป็นแค่ชื่อ** ที่ตั้งเอง (เช่น `iot_org` / `iot_bucket`)
> ไม่ต้องสุ่ม — มีแค่ **TOKEN** เท่านั้นที่ต้องสร้างแบบสุ่ม

### ต้องทำ: สร้าง 64-character hex string

### วิธีที่ 1: Node.js (ทุก platform — แนะนำ)

```bash
node -e "console.log(require('crypto').randomBytes(32).toString('hex'))"
```

### Windows PowerShell

**ใช้ PowerShell command:**

```powershell
[System.Guid]::NewGuid().ToString().Replace("-","") + [System.Guid]::NewGuid().ToString().Replace("-","")
```

**Step-by-step:**
```powershell
# 1. เปิด PowerShell

# 2. Copy & Paste:
[System.Guid]::NewGuid().ToString().Replace("-","") + [System.Guid]::NewGuid().ToString().Replace("-","")

# 3. Press Enter

# Output ควรจะเป็น:
# 8f3a2b1c9d7e5f4a6b2c1d9e8f7a3b2c8f3a2b1c9d7e5f4a6b2c1d9e8f7a3b2c
```

**ใส่ลงใน .env:**
```env
INFLUXDB_ADMIN_TOKEN=8f3a2b1c9d7e5f4a6b2c1d9e8f7a3b2c8f3a2b1c9d7e5f4a6b2c1d9e8f7a3b2c
```

### Mac/Linux Terminal

**วิธีที่ 1: ใช้ openssl**

```bash
openssl rand -hex 32
```

**Step-by-step:**
```bash
# 1. เปิด Terminal

# 2. Copy & Paste:
openssl rand -hex 32

# 3. Press Enter

# Output ควรจะเป็น:
# 8f3a2b1c9d7e5f4a6b2c1d9e8f7a3b2c8f3a2b1c9d7e5f4a6b2c1d9e8f7a3b2c
```

**ใส่ลงใน .env:**
```env
INFLUXDB_ADMIN_TOKEN=8f3a2b1c9d7e5f4a6b2c1d9e8f7a3b2c8f3a2b1c9d7e5f4a6b2c1d9e8f7a3b2c
```

### ⚠️ ตรวจสอบ Token

```
✓ ต้องยาว: 64 ตัวอักษร
✓ ต้องเป็น: hex (0-9, a-f)
✓ ไม่มี: dash (-) หรือเครื่องหมายพิเศษ
```

---

## 📋 สรุป & Next Steps

### สิ่งที่สร้างแล้ว

```
1. NODERED_PASSWORD_HASH ✓
   (จาก npx bcryptjs — escape $$ แล้ว)

2. N8N_ENCRYPTION_KEY ✓
   (32-char hex from PowerShell/openssl)

3. INFLUXDB_ADMIN_TOKEN ✓
   (64-char hex from PowerShell/openssl)
```

### ตอนนี้ .env ควรมี

```env
# ── Node-RED Login ────────────────────────────────────────
# ⚠️ hash ต้อง escape $ เป็น $$ (ดู Step 3 ด้านบน)
NODERED_USERNAME=admin
NODERED_PASSWORD_HASH=$$2b$$08$$e9VVIc9vJpnsw9LotxfoeOB.EfPAxbZ39HDts2DEbY38Gm5M2r7X2

# ── Cloudflare Tunnel (Optional) ──────────────────────────
CLOUDFLARE_TUNNEL_TOKEN=your-cloudflare-tunnel-token-here
N8N_DOMAIN=n8n.yourdomain.com

# ── N8N (SQLite Database) ────────────────────────────
N8N_ENCRYPTION_KEY=8f3a2b1c9d7e5f4a6b2c1d9e8f7a3b2c

# ── InfluxDB ──────────────────────────────────────────────
INFLUXDB_USERNAME=admin
INFLUXDB_PASSWORD=change-this-strong-password
INFLUXDB_ORG=iot_org
INFLUXDB_BUCKET=iot_bucket
INFLUXDB_ADMIN_TOKEN=8f3a2b1c9d7e5f4a6b2c1d9e8f7a3b2c8f3a2b1c9d7e5f4a6b2c1d9e8f7a3b2c

# ── Grafana ───────────────────────────────────────────────
GRAFANA_ADMIN_USER=admin
GRAFANA_ADMIN_PASSWORD=change-this-strong-password

# ── Timezone ──────────────────────────────────────────────
TZ=Asia/Bangkok
```

### Step ถัดไป

1. ✅ สร้าง credentials (ทำเสร็จแล้ว)
2. ✅ แก้ไข .env ด้วยค่าที่สร้าง
3. 🚀 Start Docker Desktop:
   ```bash
   docker compose up -d
   ```
4. 🌐 ทดสอบ services:
   - http://localhost:1880 (Node-RED)
   - http://localhost:5678 (N8N)
   - http://localhost:8086 (InfluxDB)
   - http://localhost:3000 (Grafana)

> 📖 ขั้นตอนสร้างระบบเต็ม ๆ (รวมตั้งค่า InfluxDB / Grafana / Cloudflare) ดู **[README.md](README.md)**

---

## ❓ Troubleshooting

### ❌ Node-RED login ไม่ได้ (รหัสถูกแต่เข้าไม่ได้)

**สาเหตุ:** ไม่ได้ escape `$` เป็น `$$` ใน `.env` → hash ที่ส่งเข้า container ขาด

**แก้ไข:**
```bash
docker exec nodered printenv NODERED_PASSWORD_HASH   # ต้องได้ 60 ตัว
```
- ถ้าสั้นกว่า 60 → แก้ `.env` ให้ `$` ทุกตัวเป็น `$$` แล้ว `docker compose up -d nodered`
- ห้ามใช้ `docker compose restart` (ไม่โหลดค่า .env ใหม่)

### ❌ "npm: command not found"

**แก้ไข:**
- ติดตั้ง Node.js จาก https://nodejs.org/
- Restart Terminal
- ลอง command อีกครั้ง

### ❌ "openssl: command not found" (Windows)

**แก้ไข:**
- ใช้ PowerShell command แทน:
  ```powershell
  [System.Guid]::NewGuid().ToString().Replace("-","")
  ```
- หรือติดตั้ง Git Bash

### ❌ Hash/Token เสียหาย

**แก้ไข:**
- รันคำสั่งอีกครั้ง
- Copy-paste output ใหม่

---

## 📖 Reference

- [README.md](README.md) — คู่มือสร้างระบบทั้งหมด
- [bcryptjs (npm)](https://www.npmjs.com/package/bcryptjs)
- [node-red-admin GitHub](https://github.com/node-red/node-red-admin)
- [Node.js Official](https://nodejs.org/)
