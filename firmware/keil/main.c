/* ============================================================================
 *  实验6 · 引线电感自动检测仪 (进阶版) —— Keil C51 版 (AT89C51 @12MHz)
 *  与 main_sdcc.c 逻辑一致; 实际烧写的 Objects/111.hex 由 SDCC 版编译
 *  ---------------------------------------------------------------
 *  P0    = LCD1602 数据 (需接 排阻上拉)
 *  P2.0/2.1/2.2 = LCD 的 RS/RW/E
 *  P3.5/T1 = 频率输入 (LC振荡/比较器输出方波)
 *  P1.0  = 舵机 PWM (1.0ms=0°, 1.5ms=90°, 50Hz)
 *  P1.1  = 按键1 (+)
 *  P1.2  = 按键2 (-)
 *
 *  功能:
 *    - 默认检测 1.00mH, 容差 ±5%
 *    - 5 次中值滤波
 *    - 合格 -> 舵机 90°;  不合格 -> 0°
 *    - 短按 ±0.1mH; 长按 1 秒后每 0.2s 连发 ±0.5mH
 *    - 双键同时按住 2 秒 = 标准件校准 (放标准件并把 S 设成其标称值)
 *    - LCD 显示 L / S / 合格 / 不合格
 * ==========================================================================*/
#include <reg51.h>

#define uchar unsigned char
#define uint  unsigned int

/* ---- 引脚 ---- */
sbit RS    = P2^0;
sbit RW    = P2^1;
sbit E     = P2^2;
sbit SERVO = P1^0;
sbit KEY_UP= P1^1;
sbit KEY_DN= P1^2;

/* ---- 参数 ---- */
#define GATE_TICKS  50                /* 闸门 1s = 50 x 20ms */
/* T0 = 20ms (舵机标准 50Hz 刷新) */
#define T0_RELOAD   (65536-20000)

/* 校准常数: 默认 5000^2 (对应 1s 闸门下 1mH 标准件约 5000Hz);
   可用标准件重标 (双键按住 2 秒) */
float g_cal = 25000000.0;
volatile uchar g_do_cal = 0;

int   target_x100 = 100;              /* 1.00mH */
float inductance  = 0.0;

volatile uint gate_cnt  = 0;
bit           gate_done = 0;
volatile uint kh_up = 0, kh_dn = 0;
volatile uint dbg_gates = 0, dbg_loops = 0;
uchar servo_pos = 0;

/* 自定义汉字 0=合 1=格 2=不 */
uchar code HZ[3][8] = {
    {0x04,0x0A,0x11,0x1F,0x11,0x11,0x1F,0x00},  /* 合 */
    {0x0A,0x1F,0x04,0x1F,0x15,0x1F,0x11,0x00},  /* 格 */
    {0x1F,0x04,0x0E,0x15,0x04,0x04,0x04,0x00}   /* 不 */
};

void delay_ms(uint ms){ volatile uint i,j; for(i=0;i<ms;i++) for(j=0;j<120;j++); }

void lcd_dly(void){ volatile uchar k; for(k=0;k<25;k++){ } }   /* ~227us @12MHz */
void lcd_cmd(uchar c){ RS=0; RW=0; P0=c; lcd_dly(); E=1; lcd_dly(); E=0; }
void lcd_dat(uchar d){ RS=1; RW=0; P0=d; lcd_dly(); E=1; lcd_dly(); E=0; }
void lcd_init(void){ delay_ms(25); lcd_cmd(0x38); lcd_cmd(0x0C); lcd_cmd(0x06); lcd_cmd(0x01); delay_ms(5); }
void lcd_xy(uchar x, uchar y){ lcd_cmd((y?0xC0:0x80)+x); }
void lcd_hz(void){ uchar i,j; lcd_cmd(0x40); for(i=0;i<3;i++) for(j=0;j<8;j++) lcd_dat(HZ[i][j]); }

/* T0 中断: 舵机PWM(定时器计时, 与编译器代码生成无关) + 闸门计时 + 按键计时 */
void T0_ISR(void) interrupt 1 using 1
{
    uint t0now, t0end;
    uchar tl1, th, tl2;

    TH0 = T0_RELOAD/256; TL0 = T0_RELOAD%256;
    SERVO = 1;
    t0end = T0_RELOAD + (servo_pos ? 1500 : 1000);   /* 1.5ms(90°)/1.0ms(0°) */
    do {
        tl1 = TL0; th = TH0; tl2 = TL0;              /* 防 TH0/TL0 撕裂读 */
        if (tl2 < tl1) th++;
        t0now = ((uint)th << 8) | tl2;
    } while (t0now < t0end);
    SERVO = 0;

    if (++gate_cnt >= GATE_TICKS){ gate_cnt = 0; gate_done = 1; dbg_gates++; }
    if (!KEY_UP){ if (kh_up < 60000) kh_up++; } else kh_up = 0;
    if (!KEY_DN){ if (kh_dn < 60000) kh_dn++; } else kh_dn = 0;
}

/* 中值滤波 (全局数组, 避免指针/存储模式问题) */
uint fbuf[5] = {0,0,0,0,0}, sortbuf[5];
uint median5(void)
{
    uchar i, j; uint t;
    for(i=0;i<5;i++) sortbuf[i] = fbuf[i];
    for(i=0;i<4;i++) for(j=0;j<4-i;j++)
        if(sortbuf[j] > sortbuf[j+1]){ t=sortbuf[j]; sortbuf[j]=sortbuf[j+1]; sortbuf[j+1]=t; }
    return sortbuf[2];
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
    uint  fm;
    float total, L;
    int   lx, lo, hi, last_tgt;

    TMOD = 0x51;                      /* T1 计数器(P3.5), T0 定时器 */
    ET0 = 0; TR0 = 0; TR1 = 0; EA = 0;

    lcd_init();                       /* 先初始化 LCD (此时无中断) */
    lcd_hz();

    TH0 = T0_RELOAD/256; TL0 = T0_RELOAD%256;
    ET0 = 1; EA = 1; TR0 = 1;         /* 再开 T0 (舵机PWM+闸门) */

    while(1)
    {
        /* --- 开始一次测量: 窗口严格 = GATE_TICKS * 20ms = 1s --- */
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
        fm = median5();

        if (fm > 100){
            total = 2.0 * g_cal / ((float)fm * (float)fm);
            L = total - 1.0 - 0.01;
            if (L < 0.0) L = 0.0;
            L = ((int)(L*100.0)) / 100.0;
        } else L = 0.0;
        inductance = L;

        if (g_do_cal && (fm > 100)) {           /* 用标准件重标校准常数 */
            float tgt = (float)target_x100 * 0.01;
            g_cal = (tgt + 1.01) * 0.5 * (float)fm * (float)fm;
            g_do_cal = 0;
            lcd_xy(11,1); lcd_dat('C'); lcd_dat('A'); lcd_dat('L');
        }

        lx = (int)(L*100.0 + 0.5);
        lo = target_x100 * 95 / 100;
        hi = target_x100 * 105 / 100;
        ok = (lx >= lo) && (lx <= hi);

        show(lx, target_x100, ok);

        servo_pos = ok ? 1 : 0;       /* 合格 90°, 不合格 0° */

        key_scan();
        dbg_loops++;
    }
}
