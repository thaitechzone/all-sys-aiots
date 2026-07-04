module.exports = {
    // ─── Web UI ──────────────────────────────────────────────
    uiPort: process.env.PORT || 1880,
    uiHost: "0.0.0.0",

    // ─── Security ────────────────────────────────────────────
    // ใช้ environment variables สำหรับ credentials
    adminAuth: process.env.NODERED_USERNAME && process.env.NODERED_PASSWORD_HASH ? {
        type: "credentials",
        users: [{
            username: process.env.NODERED_USERNAME || "admin",
            password: process.env.NODERED_PASSWORD_HASH,
            permissions: "*"
        }]
    } : undefined,

    // ─── Timezone ────────────────────────────────────────────
    timezone: "Asia/Bangkok",

    // ─── Logging ─────────────────────────────────────────────
    logging: {
        console: {
            level: "info",
            metrics: false,
            audit: false
        }
    },

    // ─── Editor ──────────────────────────────────────────────
    editorTheme: {
        projects: {
            enabled: false
        },
        palette: {
            // nodes ที่ติดตั้งเพิ่ม
        }
    },

    // ─── Context Storage ─────────────────────────────────────
    contextStorage: {
        default: {
            module: "memory"
        },
        file: {
            module: "localfilesystem"
        }
    },

    // ─── Function Global Context ──────────────────────────────
    // ค่าที่ใช้ได้ใน Function node ทุกตัว
    functionGlobalContext: {
        OPC_SERVER: `opc.tcp://${process.env.OPC_SERVER_IP || '192.168.1.100'}:${process.env.OPC_SERVER_PORT || '4840'}`
    },

    // ─── Node Settings ────────────────────────────────────────
    debugMaxLength: 1000,
    mqttReconnectTime: 15000,
    serialReconnectTime: 15000,
}
