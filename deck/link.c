/* link.c: the Bluetooth link between two DECKs (included by app.c, before fields.c).
 *
 * What it does: with BT ON it keeps the radio's scan and the central link to the one partner (an FM-1 whose address
 * is saved in the settings) going, and shows the player what is going on (the GLOBAL BT row). The clock and the
 * transport themselves travel as plain BLE MIDI (follow.c, ui.c's midi_in): this file only makes the link.
 *
 * - Two FM-1 both advertise "FM-1": the partner is told by its ADDRESS (set.btaddr, saved when the player picks it).
 * - Who connects (both sides would otherwise connect to each other, and two links hold the radio twice): the unit
 *   whose own address is the higher one. The other waits LK_FALLBACK ms with no link, then connects itself (the
 *   first has not come). A unit that already has the partner as a central (plat_ble_connected) while it also holds
 *   the central link: the lower address drops its central link.
 * - It never starts the radio by itself: only the player's BT ON does (plat_ble_enable). set.bt is the player's
 *   wish; if it is on and the radio is off at the start (the platform did not start it: the last Bluetooth run
 *   crashed), set.bt goes back to 0 (link_init) and the page says so.
 * - It scans only while the BT row is up (to show the list), and for a saved partner in bursts (LK_SCAN_ON / LK_SCAN_OFF).
 * link_poll() is called every UI frame. */

static void lk_cp(void *d, const void *s, int n)    /* (no libc here: the app brings its own) */
{
    uint8_t *dd = d;
    const uint8_t *ss = s;
    while (n-- > 0)
        *dd++ = *ss++;
}
static void lk_zero(void *d, int n)
{
    uint8_t *dd = d;
    while (n-- > 0)
        *dd++ = 0;
}

#define LK_DWELL 1200u                   /* the DEVICE knob rests this long on a device: it connects */
#define LK_FALLBACK 8000u                /* the lower-address unit connects itself after this long with no link */
#define LK_SCAN_ON 6000u                 /* looking for the partner (the page not up): this long, ... */
#define LK_SCAN_OFF 9000u                /* ... then quiet this long */
#define LK_REDO 1500u                    /* after a link is lost: looking again after this long */
#define LK_MAXDEV 8

static void say(const char *a, const char *b);

enum { LKS_OFF, LKS_START, LKS_FAIL, LKS_IDLE, LKS_SCAN, LKS_CONN, LKS_LINK, LKS_IN };   /* what the page shows */

static struct {
    uint8_t own[6];
    uint8_t own_ok;
    uint8_t scanning;                    /* a scan asked for by this file */
    uint8_t was;                         /* the central link was up */
    uint8_t pick;                        /* the DEVICE knob: 0 NONE, 1.. the list's device + 1 */
    uint8_t pick_pend;                   /* a pick resting: connects at pick_t + LK_DWELL */
    uint8_t user;                        /* the connection under way is the player's pick: saved once up */
    uint8_t crash;                       /* the BT was switched off at the start for the last run's crash */
    uint8_t tried;                       /* this scan: a connect to the partner was asked for */
    uint32_t pick_t, idle_t, scan_t, next_t, try_t;
} lk;

static int lk_found(int i, char *nm, int *rssi, uint8_t a[7])
{
    char n[24];
    int r = 0;
    if (i < 0 || i >= LK_MAXDEV || !plat_ble_found(i, n, &r))
        return 0;
    if (nm)
        lk_cp(nm, n, 21);
    if (rssi)
        *rssi = r;
    if (a && !plat_ble_found_addr(i, a))
        lk_zero(a, 7);
    return 1;
}
static int lk_count(void)
{
    int n = 0;
    while (n < LK_MAXDEV && lk_found(n, 0, 0, 0))
        n++;
    return n;
}
static int lk_is_partner(const uint8_t a[7]) { return set.btp && !memcmp(a, set.btaddr, 7); }
static int lk_index_of_partner(void)
{
    uint8_t a[7];
    int i;
    for (i = 0; i < LK_MAXDEV && lk_found(i, 0, 0, a); i++)
        if (lk_is_partner(a))
            return i;
    return -1;
}
static int lk_initiator(void)            /* this unit connects first (its address is the higher; no address: nobody does) */
{
    return lk.own_ok && set.btp && memcmp(lk.own, set.btaddr + 1, 6) > 0;
}
static int lk_central(void) { return plat_ble_device() == BLE_DEV_READY; }

/* the state for the page's status line and the header's dot */
static int link_status(void)
{
    int st;
    if (!set.bt)
        return LKS_OFF;
    st = plat_ble_state();
    if (st == BLE_FAILED)
        return LKS_FAIL;
    if (st != BLE_READY)
        return st == BLE_STARTING ? LKS_START : LKS_OFF;
    if (lk_central())
        return LKS_LINK;
    if (plat_ble_connected())
        return LKS_IN;
    st = plat_ble_device();
    if (st == BLE_DEV_CONNECTING || st == BLE_DEV_SEARCHING)
        return LKS_CONN;
    return st == BLE_DEV_SCANNING ? LKS_SCAN : LKS_IDLE;
}

static void lk_scan(int on)
{
    if (on == lk.scanning)
        return;
    lk.scanning = (uint8_t)on;
    lk.tried = 0;
    lk.scan_t = plat_ms();
    plat_ble_scan(on);
}

/* BT ON / OFF (the page's K1) */
static void link_set_bt(int on)
{
    if (on == (set.bt != 0))
        return;
    set.bt = (uint8_t)on;
    settings_save_later();
    lk.idle_t = plat_ms();
    lk.next_t = 0;
    lk.crash = 0;
    if (on) {
        plat_ble_enable(1);              /* (it may hold the sound for a second: the radio starts) */
    } else {
        lk_scan(0);
        plat_ble_disconnect();
        plat_ble_enable(0);
        lk.was = 0;
        lk.pick_pend = 0;
    }
}
/* DEVICE (the page's K3): 0 NONE (forget the partner), else the list's device; it connects once the knob rests */
static void link_pick(int v)
{
    lk.pick = (uint8_t)v;
    lk.pick_pend = 1;
    lk.pick_t = plat_ms();
}
static int link_pick_get(void)
{
    int i;
    if (lk.pick_pend || (lk.user && !set.btp))
        return lk.pick;
    if (!set.btp)
        return 0;
    i = lk_index_of_partner();           /* the partner: its place in the list, or 1 ("saved", not in view) */
    return i >= 0 ? i + 1 : 1;
}
static int link_pick_max(void)
{
    int n = lk_count();
    return n < 1 && set.btp ? 1 : n;
}
static char *lk_hex2(char *o, int v)
{
    *o++ = "0123456789ABCDEF"[v >> 4 & 15];
    *o++ = "0123456789ABCDEF"[v & 15];
    return o;
}
/* a device's label: its name and the address's last two bytes (two FM-1 have one name); v: 0 NONE, 1.. the list */
static void link_dev_name(int v, char *o, int n)
{
    char nm[24], t[40], *p = t;
    uint8_t a[7];
    int i;
    if (v <= 0) {
        lk_cp(t, "NONE", 5);
    } else if (lk_found(v - 1, nm, 0, a)) {
        for (i = 0; nm[i] && i < 12; i++)
            *p++ = nm[i];
        *p++ = ' ';
        p = lk_hex2(p, a[5]);
        *p++ = ':';
        p = lk_hex2(p, a[6]);
        *p = 0;
    } else if (set.btp) {                /* saved, not in view */
        lk_cp(t, "SAVED ", 6);
        p = lk_hex2(t + 6, set.btaddr[5]);
        *p++ = ':';
        p = lk_hex2(p, set.btaddr[6]);
        *p = 0;
    } else {
        lk_cp(t, "-", 2);
    }
    for (i = 0; i < n - 1 && t[i]; i++)
        o[i] = t[i];
    o[i] = 0;
}

static void link_init(void)
{
    lk_zero(&lk, sizeof lk);
    if (set.btrole > 2)
        set.btrole = 0;
    if (set.bt && plat_ble_state() == BLE_OFF) {   /* wanted on, and the radio is off: the platform kept it off after a crash */
        set.bt = 0;
        lk.crash = 1;
        settings_save_later();
    }
    plat_ble_own(lk.own);
    lk.own_ok = (uint8_t)(lk.own[0] | lk.own[1] | lk.own[2] | lk.own[3] | lk.own[4] | lk.own[5]) != 0;
    lk.idle_t = plat_ms();
}

static void link_poll(int page_up)
{
    uint32_t now = plat_ms();
    int st, dev, c;
    uint8_t a[7];
    if (!set.bt || plat_ble_state() != BLE_READY) {
        lk.scanning = 0;
        lk.was = 0;
        return;
    }
    if (!lk.own_ok) {                    /* (the address is known once the radio is up) */
        plat_ble_own(lk.own);
        lk.own_ok = (uint8_t)(lk.own[0] | lk.own[1] | lk.own[2] | lk.own[3] | lk.own[4] | lk.own[5]) != 0;
    }
    dev = plat_ble_device();
    c = dev == BLE_DEV_READY;
    if (c && !lk.was) {                  /* the central link came up: the player's pick becomes the partner */
        lk.was = 1;
        lk.scanning = 0;
        if (lk.user && plat_ble_peer(a)) {
            lk_cp(set.btaddr, a, 7);
            set.btp = 1;
            settings_save_later();
            say("BT", "PARTNER SAVED");
        }
        lk.user = 0;
    } else if (!c && lk.was) {           /* lost: look again soon (idle since now) */
        lk.was = 0;
        lk.idle_t = now;
        lk.next_t = now + LK_REDO;
    }
    if (dev == BLE_DEV_OFF || dev == BLE_DEV_SCANNING)
        lk.scanning = dev == BLE_DEV_SCANNING;
    if (c) {
        if (plat_ble_connected() && set.btp && lk.own_ok && !lk_initiator()) {   /* both ways: the lower address drops its */
            plat_ble_disconnect();
            lk.idle_t = now;
        }
        return;
    }
    if (dev == BLE_DEV_CONNECTING || dev == BLE_DEV_SEARCHING) {
        if (now - lk.try_t > 20000u) {   /* (the library gives up itself; this is the net under it) */
            plat_ble_disconnect();
            lk.user = 0;
        }
        return;
    }
    /* the player's pick, rested */
    if (lk.pick_pend && now - lk.pick_t >= LK_DWELL) {
        lk.pick_pend = 0;
        if (!lk.pick) {
            if (set.btp) {
                set.btp = 0;
                lk_zero(set.btaddr, 7);
                settings_save_later();
                say("BT", "PARTNER FORGOTTEN");
            }
        } else if (lk_found(lk.pick - 1, 0, 0, a)) {
            if (plat_ble_linked() && !plat_ble_connected()) {
                say("BT", "A LINK IS BEING MADE");   /* (a central is connecting to this unit) */
            } else {
                lk.user = 1;
                lk.try_t = now;
                lk_scan(0);
                plat_ble_connect(lk.pick - 1);
                return;
            }
        } else {
            say("BT", "DEVICE GONE");
        }
    }
    /* looking for the partner (the page not up: in bursts) */
    st = set.btp && !plat_ble_linked() && now >= lk.next_t && (lk_initiator() || now - lk.idle_t >= LK_FALLBACK);
    if (page_up || st) {
        if (!lk.scanning && (page_up || now >= lk.next_t)) {
            lk_scan(1);
        } else if (lk.scanning && !page_up && now - lk.scan_t > LK_SCAN_ON) {
            lk_scan(0);
            lk.next_t = now + LK_SCAN_OFF;
        }
    } else if (lk.scanning) {
        lk_scan(0);
    }
    if (lk.scanning && st && !lk.tried) {          /* the partner in the list: connect */
        int i = lk_index_of_partner();
        if (i >= 0) {
            lk.tried = 1;
            lk.try_t = now;
            lk.user = 0;
            lk_scan(0);
            plat_ble_connect(i);
        }
    }
}

/* the status line (n bytes) for the page */
static void link_text(char *o, int n)
{
    static const char *const S[] = {"BT OFF", "STARTING...", "BT FAILED", "READY", "SCANNING", "CONNECTING...", "LINKED", "LINKED (THEY CALLED)"};
    int s = link_status(), i = 0;
    const char *t = s == LKS_OFF && lk.crash ? "OFF: THE LAST RUN CRASHED" : S[s];
    while (i < n - 1 && t[i]) {
        o[i] = t[i];
        i++;
    }
    o[i] = 0;
}
