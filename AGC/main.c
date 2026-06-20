// 程序用途：AGC板调试程序 + 串口数据显示
// 计时基准：5ms (ADC采样)
// 串口配置：波特率 9600, 8位数据, 无校验
// 硬件提示：确保 LaunchPad 上的 RXD/TXD 跳线帽处于正立（UART）模式

#include <msp430g2553.h>

//////////////////////////////
//      常量伪指令定义       //
//////////////////////////////
#define V_T100ms    20   // 100ms
#define V_T500ms   100   // 500ms
#define V_T1000ms  200   // 1s (200 * 5ms)
#define AVG_TIMES   10   

#define LD_H    P2OUT|=BIT2   
#define LD_L    P2OUT&=~BIT2
#define CLK_H   P2OUT|=BIT0   
#define CLK_L   P2OUT&=~BIT0
#define SRI_H   P2OUT|=BIT1   
#define SRI_L   P2OUT&=~BIT1

#define V_GAIN0_5   2047    
#define V_GAIN1_0   4095    

#define ADC_CH_SD1  INCH_4  
#define ADC_CH_SD2  INCH_5  

#define KEY_ACTIVE   ((P1IN&BIT3)==0)
#define ACTIVE    1
#define INACTIVE  0
#define LED1_ON   P1OUT|=BIT0
#define LED1_OFF  P1OUT&=~BIT0
#define LED2_ON   P1OUT|=BIT6
#define LED2_OFF  P1OUT&=~BIT6

//////////////////////////////
//      全局性变量定义       //
//////////////////////////////
unsigned char sys_state = 1;      
unsigned char key_pre = INACTIVE;
unsigned int  cnt_500ms = 0;      
unsigned char clock500ms_flag = 0;
unsigned int  cnt_100ms = 0;
unsigned int  cnt_1000ms = 0;    // 1秒计时器
unsigned char clock1s_flag = 0;   // 1秒触发标志

// ADC 变量
unsigned long adc1_sum = 0;       
unsigned long adc2_sum = 0;
unsigned int  adc_sample_cnt = 0; 
unsigned int  adc1_avg = 0;       
unsigned int  adc2_avg = 0;
unsigned int  code = 0;
//////////////////////////////
//      UART 串口功能        //
//////////////////////////////

// 初始化 UART 9600 波特率 (基于 SMCLK = 2MHz)
void UART_Init(void)
{
    P1SEL |= BIT1 + BIT2;            // P1.1 = RXD, P1.2 = TXD
    P1SEL2 |= BIT1 + BIT2;           // 必须两行都写，指向硬件USCI模块
    
    UCA0CTL1 |= UCSWRST;             // 状态机复位
    UCA0CTL1 |= UCSSEL_2;            // 选择 SMCLK (2MHz)
    
    // 分频系数计算: 2,000,000 / 9600 = 208.33
    UCA0BR0 = 208;                   
    UCA0BR1 = 0;
    UCA0MCTL = UCBRS_0;              // 调制系数
    
    UCA0CTL1 &= ~UCSWRST;            // 启动 UART
    
   
    IFG2 &= ~UCA0RXIFG; 
}

// 发送一个字节
void UART_SendChar(char c)
{
    while (!(IFG2 & UCA0TXIFG));     // 等待发送缓冲区空
    UCA0TXBUF = c;
}

// 发送字符串
void UART_SendString(char* s)
{
    while (*s) UART_SendChar(*s++);
}

// 发送整数 (0-1023)
void UART_SendInt(unsigned int num)
{
    UART_SendChar(num / 1000 + '0');
    UART_SendChar((num % 1000) / 100 + '0');
    UART_SendChar((num % 100) / 10 + '0');
    UART_SendChar(num % 10 + '0');
}

//////////////////////////////
//      基础模块初始化       //
//////////////////////////////

void ADC10_Init(void)
{
    ADC10CTL0 = ADC10SHT_2 + ADC10ON; 
    ADC10CTL1 = ADC_CH_SD2 + ADC10SSEL_3; 
    ADC10AE0 |= BIT4 + BIT5; 
}

unsigned int Get_ADC_Raw(unsigned int channel)
{
    ADC10CTL0 &= ~ENC;            
    ADC10CTL1 = channel + ADC10SSEL_3;      
    ADC10CTL0 |= ENC + ADC10SC;             
    while (ADC10CTL1 & ADC10BUSY);          
    return ADC10MEM;                        
}

void Port_Init(void)
{
    P2SEL &= ~(BIT7+BIT6);       
    P2DIR |= 0xff;               
    P1DIR |= 0xff;               
    P1DIR &= ~(BIT1 + BIT3 + BIT4 + BIT5); // P1.1为串口输入
    P1REN |= BIT3;               
    P1OUT |= BIT3;
}

void Timer0_Init(void)
{
    TA0CTL = TASSEL_2 + MC_1 ;      
    TA0CCR0 = 10000;                // 5ms (2MHz SMCLK)
    CCTL0 = CCIE;                   
}

void Init_Devices(void)
{
    WDTCTL = WDTPW + WDTHOLD;     
    if (CALBC1_16MHZ == 0xFF || CALDCO_16MHZ == 0xFF) { while(1); }

    BCSCTL1 = CALBC1_16MHZ;      
    DCOCTL = CALDCO_16MHZ;    
    BCSCTL3 |= LFXT1S_2;     
    IFG1 &= ~OFIFG;          
    BCSCTL2 |= DIVS_3;       // SMCLK = 2MHz

    Port_Init();             
    Timer0_Init();          
    ADC10_Init();            
    UART_Init();             // 初始化串口
    _BIS_SR(GIE);            
}

void DAC8043(unsigned int data1)
{
    unsigned char i1;
    LD_L; CLK_L; LD_H;
    for (i1=0; i1<12; i1++)
    {
        CLK_L;
        if ((data1 & 0x0800) == 0) SRI_L;
        else SRI_H;
        data1 <<= 1;
        CLK_H;
    }
    CLK_L; LD_L;
}

//////////////////////////////
//      中断服务程序         //
//////////////////////////////

#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer0_A0 (void)
{
    // ADC采样累加 (5ms/次)
    adc1_sum += Get_ADC_Raw(ADC_CH_SD1);
    adc2_sum += Get_ADC_Raw(ADC_CH_SD2);
    adc_sample_cnt++;

    if (adc_sample_cnt >= AVG_TIMES) // 50ms更新一次均值
    {
        adc1_avg = adc1_sum / AVG_TIMES;
        adc2_avg = adc2_sum / AVG_TIMES;
        adc1_sum = 0;
        adc2_sum = 0;
        adc_sample_cnt = 0;
    }

    // 1秒显示定时器
    if (++cnt_1000ms >= V_T1000ms)
    {
        cnt_1000ms = 0;
        clock1s_flag = 1;
    }

    // 500ms DAC控制定时器
    if (++cnt_500ms >= V_T500ms)
    {
        cnt_500ms = 0;
        clock500ms_flag = 1;
    }

    // 100ms 按键检测
    if (++cnt_100ms >= V_T100ms)
    {
        cnt_100ms = 0;
        if (KEY_ACTIVE)
        {
            if (key_pre == INACTIVE)
            {
                if (sys_state == 1) sys_state = 2;
                else sys_state = 1;
            }
            key_pre = ACTIVE;
        }
        else key_pre = INACTIVE;
    }
}

//////////////////////////////
//         主程序           //
//////////////////////////////
int main(void)
{
    volatile unsigned char ii;
    for (ii=0; ii<=250; ii++); 
    Init_Devices();

    // 开机欢迎词
    UART_SendString("AGC System Initialized\r\n");

    while(1)
    {
        // 每500ms执行一次状态控制
        if (clock500ms_flag == 1)
        {
            clock500ms_flag = 0;
            if (sys_state == 1) { 
                LED1_ON; LED2_OFF; 
                //DAC8043(V_GAIN0_5); 
            }
            else { 
                LED1_OFF; LED2_ON; 
            //DAC8043(V_GAIN1_0);
            
             }
             //if(adc1_avg<=27) DAC8043(4095);
             //else if(adc1_avg>250&&adc1_avg<280)DAC8043(819);
             //else if(adc1_avg>590)DAC8043(410);
             code = (int)(819200.0/(2.9*adc1_avg+196.2897));
             DAC8043(code);
        }

        // 每1秒向串口发送一次数据
        if (clock1s_flag == 1)
        {
            clock1s_flag = 0;
            
            UART_SendString("ADC1 (P1.4): ");
            UART_SendInt(adc1_avg);
            UART_SendString(" | ADC2 (TP6): ");
            UART_SendInt(adc2_avg);
            
            if (sys_state == 2) UART_SendString(" [1Vpp Calibrating...]");
            
            UART_SendString("\r\n");
        }
    }
}
