/* ============================================================================
 *  实验6 · 引线电感自动检测仪 (进阶版) —— SDCC 版 (AT89C51 @12MHz)
 *  P0=LCD数据  P2.0/2.1/2.2=RS/RW/E
 *  P3.5/T1=频率输入  P1.0=舵机  P1.1=按键+  P1.2=按键-
 *  默认 1.00mH, 容差 ±5%, 5次中值滤波, 合格->舵机90°, 不合格->0°, 中文显示
 * ==========================================================================*/
#include <8051.h>

#define uchar unsigned char
#define uint  unsigned int

__sbit __at (0xA0) RS;      /* P2.0 */
__sbit __at (0xA1) RW;      /* P2.1 */
__sbit __at (0xA2) E;       /* P2.2 */
__sbit __at (0x90) SERVO;   /* P1.0 */
__sbit __at (0x91) KEY_UP;  /* P1.1 */
__sbit __at (0x92) KEY_DN;  /* P1.2 */

float g_cal = 25000000.0f;   /* = 5000^2, 可被标准件校准 */
volatile uchar g_do_cal = 0;
#define GATE_TICKS  50                /* 1s = 50 x 20ms (T0=20ms: 舵机标准50Hz) */

int   target_x100 = 100;              /* 1.00mH */
float inductance  = 0.0f;

volatile uint gate_cnt  = 0;
volatile __bit gate_done = 0;
volatile uint servo_cnt = 0;
volatile uint kh_up = 0, kh_dn = 0;
volatile uint dbg_gates = 0;
volatile uint dbg_loops = 0;
uchar servo_pos = 0;

/* 自定义汉字 0=合 1=格 2=不 */
__code uchar HZ[3][8] = {
    {0x04,0x0A,0x11,0x1F,0x11,0x11,0x1F,0x00},  /* 合 */
    {0x0A,0x1F,0x04,0x1F,0x15,0x1F,0x11,0x00},  /* 格 */
    {0x1F,0x04,0x0E,0x15,0x04,0x04,0x04,0x00}   /* 不 */
};

void delay_ms(uint ms){ volatile uint i,j; for(i=0;i<ms;i++) for(j=0;j<120;j++); }

void lcd_dly(void){ volatile uchar k; for(k=0;k<25;k++){ } }   /* ~20us */
void lcd_cmd(uchar c){ RS=0; RW=0; P0=c; lcd_dly(); E=1; lcd_dly(); E=0; }
void lcd_dat(uchar d){ RS=1; RW=0; P0=d; lcd_dly(); E=1; lcd_dly(); E=0; }
void lcd_init(void){ delay_ms(25); lcd_cmd(0x38); lcd_cmd(0x0C); lcd_cmd(0x06); lcd_cmd(0x01); delay_ms(5); }
void lcd_xy(uchar x, uchar y){ lcd_cmd((y?0xC0:0x80)+x); }
void lcd_hz(void){ uchar i,j; lcd_cmd(0x40); for(i=0;i<3;i++) for(j=0;j<8;j++) lcd_dat(HZ[i][j]); }

void T0_ISR(void) __interrupt (1)
{
    volatile unsigned int d;
    unsigned int pulse;
    TH0 = (65536-20000)/256; TL0 = (65536-20000)%256;   /* 20ms */
    SERVO = 1;
    pulse = servo_pos ? 70 : 47;    /* 真芯片21MC/次+20MC固定 => 1.490ms(90°) / 1.007ms(0°) */
    for (d = 0; d < pulse; d++) { }
    SERVO = 0;
    if (++gate_cnt >= GATE_TICKS){ gate_cnt = 0; gate_done = 1; dbg_gates++; }
    if (!KEY_UP){ if (kh_up < 60000) kh_up++; } else kh_up = 0;
    if (!KEY_DN){ if (kh_dn < 60000) kh_dn++; } else kh_dn = 0;
}

uint measure_freq(void)
{
    uint f;
    gate_done = 0;
    gate_cnt  = 0;               /* 关键: 从零开始, 窗口才严格 = GATE_TICKS*50ms */
    TH1 = 0; TL1 = 0; TR1 = 1;
    while(!gate_done);
    TR1 = 0;
    f = ((uint)TH1 << 8) | TL1;
    TH1 = 0; TL1 = 0;
    return f;
}

uint median5(uint *a)
{
    uchar i, j; uint t;
    for(i=0;i<4;i++) for(j=0;j<4-i;j++)
        if(a[j] > a[j+1]){ t=a[j]; a[j]=a[j+1]; a[j+1]=t; }
    return a[2];
}

void key_scan(void)
{
    static uchar up_prev = 1, dn_prev = 1;
    static uint  up_next = 0, dn_next = 0;
    static uchar cal_armed = 1;

    /* 双键同时按住: 只做校准计时(2秒), 不调目标, 防止长按连发乱漂 */
    if ((KEY_UP == 0) && (KEY_DN == 0)) {
        up_prev = 1; dn_prev = 1; up_next = 0; dn_next = 0;
        if (cal_armed && kh_up >= 100 && kh_dn >= 100){ g_do_cal = 1; cal_armed = 0; }
        return;
    }
    cal_armed = 1;

    if (KEY_UP == 0){
        if (up_prev == 1){ if(target_x100<1000) target_x100 += 10; up_next = 50; }
        else if (kh_up >= up_next){ if(target_x100<1000) target_x100 += 50; up_next = kh_up + 10; }
        up_prev = 0;
    } else { up_prev = 1; up_next = 0; }

    if (KEY_DN == 0){
        if (dn_prev == 1){ if(target_x100>10) target_x100 -= 10; dn_next = 50; }
        else if (kh_dn >= dn_next){ if(target_x100>10) target_x100 -= 50; dn_next = kh_dn + 10; }
        dn_prev = 0;
    } else { dn_prev = 1; dn_next = 0; }
}

void put2(uchar v){ lcd_dat('0' + (v/10)%10); lcd_dat('0' + v%10); }
void put3(uchar v){ lcd_dat('0' + (v/100)%10); lcd_dat('.'); put2(v%100); }

void show(int lx, int tgt, uchar ok)
{
    lcd_xy(0,0);
    lcd_dat('L'); lcd_dat('='); put3((uchar)lx); lcd_dat(' '); lcd_dat('m'); lcd_dat('H'); lcd_dat(' ');
    lcd_xy(0,1);
    lcd_dat('S'); lcd_dat('='); put3((uchar)tgt); lcd_dat(' ');
    if (ok){ lcd_dat(0); lcd_dat(1); }
    else   { lcd_dat(2); lcd_dat(0); lcd_dat(1); }
    lcd_dat(' ');
}

void main(void)
{
    uchar i, ok, fidx = 0, first_sample = 1;
    uint  fbuf[5] = {0,0,0,0,0}, buf[5], fm;
    float total, L;
    int   lx, lo, hi;

    TMOD = 0x51;                      /* T1 计数器(P3.5), T0 定时器 */
    ET0 = 0; TR0 = 0; TR1 = 0; EA = 0;

    lcd_init();                       /* 先初始化 LCD (此时无中断) */
    lcd_hz();

    TH0 = (65536-20000)/256; TL0 = (65536-20000)%256;   /* 20ms */
    ET0 = 1; EA = 1; TR0 = 1;         /* 再开 T0 (舵机PWM+闸门) */

    while(1)
    {
        int last_tgt;
        /* --- 开始一次测量: 窗口严格 = GATE_TICKS * 50ms --- */
        gate_done = 0; gate_cnt = 0;
        TH1 = 0; TL1 = 0; TR1 = 1;
        last_tgt = target_x100;
        while (!gate_done) {
            key_scan();                          /* 测量期间也查按键 */
            if (target_x100 != last_tgt) {       /* 按键立即刷新 S */
                last_tgt = target_x100;
                lcd_xy(0,1);
                lcd_dat('S'); lcd_dat('='); put3((uchar)target_x100);
            }
        }
        TR1 = 0;
        fbuf[fidx] = ((uint)TH1 << 8) | TL1;
        TH1 = 0; TL1 = 0;
        if (first_sample){ for(i=0;i<5;i++) fbuf[i] = fbuf[fidx]; first_sample = 0; }
        fidx = (fidx + 1) % 5;
        for(i=0;i<5;i++) buf[i] = fbuf[i];
        fm = median5(buf);

        if (fm > 100){
            total = 2.0f * g_cal / ((float)fm * (float)fm);
            L = total - 1.0f - 0.01f;
            if (L < 0.0f) L = 0.0f;
            L = ((int)(L*100.0f)) / 100.0f;
        } else L = 0.0f;
        inductance = L;

        if (g_do_cal && (fm > 100)) {           /* 用标准件重标 F_BASE */
            float tgt = (float)target_x100 * 0.01f;
            g_cal = (tgt + 1.01f) * 0.5f * (float)fm * (float)fm;
            g_do_cal = 0;
            lcd_xy(11,1); lcd_dat('C'); lcd_dat('A'); lcd_dat('L');
        }

        lx = (int)(L*100.0f + 0.5f);
        lo = target_x100 * 95 / 100;
        hi = target_x100 * 105 / 100;
        ok = (lx >= lo) && (lx <= hi);

        show(lx, target_x100, ok);

        servo_pos = ok ? 1 : 0;       /* 合格 90°, 不合格 0° */

        key_scan();
        dbg_loops++;
    }
}
