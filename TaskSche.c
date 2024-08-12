#define lineSize 2 //别真跑65534个线程，Address Overflow警告

unsigned char run;//当前运行的线程
void timer0();//挂靠函数
void timer1();

/*
threads结构体简述
status是状态，占两个字节
    0.替换至下一个任务
    1.就绪
    2.中断挂起
    3.手动挂起
    4.内存数值等于挂起（与listen比对）
    5.内存数值大于挂起
    6.内存数值小于挂起
    7.等待（listen装时间，使用T0计数）
    后面留给错误处理，处理过后标为3供后续线程写入
level是优先级，占两个字节
listen是监听端口或数值，有时也是等待数值
pt指向监听内存
*/

struct threads{
    char status;
    int pos;
    int listen;
    int* pt;
}Line[lineSize];

void forceJump(int position){
    #pragma asm
	POP B
	POP B
    MOV B,R7
    PUSH B
    MOV B,R6
    PUSH B
    #pragma endasm
}//劫持栈内ret数值，令其返回到意料之外的位置

void forceRet(int position){
    #pragma asm
		POP B
		MOV R5,B
		POP B
		MOV R4,B
        MOV B,R7
        PUSH B
        MOV B,R6
        PUSH B
        MOV B,R4
		PUSH B
		MOV B,R5
		PUSH B
    #pragma endasm
}//劫持外边一层的ret，令其返回到意料之外的位置

void reload(){
	TH0 = (65535 - T0Ti)/256;
	TL0 = (65535 - T0Ti)%256;
	TH1 = (65535 - T1Ti)/256;
	TL1 = (65535 - T1Ti)%256;
	TR0 = 1;
	TR1 = 1;
}//重载计数器

void call(int pos){
    forceJump(pos);
}//套一个CALL便于返回原位置，使用C函数的目的是便于Cx51编译优化（该死的Deadcode Elimination）

void createTask(int pos,char PID){
    char i;
    if (PID == 0) 
        for (i=0;i<lineSize;i++){
            if (Line[i].status == 3) {
                Line[i].pos = pos;
                break;
            }
    }
    else {
        Line[PID-1].pos = pos;
        Line[PID-1].status = 1;
    }
}//创建任务，如果PID=0就自动选一个PID填充，PID数值越高，优先级越高，越先执行

void hungUp(){
    char i;
    Line[run].status = 0;
    for (i=0;i<lineSize;i++) if(Line[i].status == 1) run = i;
    forceRet(Line[run].pos);
}//手动挂起一个线程

void Ready(unsigned char PID){
    Line[PID-1].status == 1;
}//手动将一个线程换为就绪状态

void Init(){
    char i;
    EA = 1;
	TMOD = 0x11;
	ET0 = 1;
	ET1 = 1;
	OUT1 = 1;
	OUT2 = 0;
    for (i=0;i<lineSize;i++) Line[i].status = 3;
    reload();
}//初始化，包括计数器和线程队列等等乱七八糟的东西

void Tsche(){
    char i,mark=1;
    while(mark){
        for (i=0;i<lineSize;i++) if(Line[i].status == 1) run = i;
        call(Line[run].pos);
        if (Line[run].status != 0) Line[run].status = 3;
        else {
            Line[run].status = 1;
            Line[run].pos = Line[run].listen;
        }
        mark = 0;
        for (i=0;i<lineSize;i++) if (Line[i].status != 3) mark = 1;
    }
}//主调度

void Next(unsigned char PID,int pos){
    Line[PID-1].listen = pos;
    Line[PID-1].status = 0;
}//替换函数，放在函数末尾，将某个线程替换为另一个线程，我觉得这个东西大有玩头

void Wait(unsigned char PID,int Time){
    Line[PID-1].status = 7;
    Line[PID=1].listen = Time;
}//等待函数，写在任务里，时间到了就会转为后续

unsigned char getPID(){
    return run+1;
}//返回当前正在执行任务的PID

void T0() interrupt 1{
    TR0=0;
    int i;
    for (i=0;i<lineSize;i++) if(Line[i].status == 7) {
        Line[i].listen = Line[i].listen - T0Time;
        if (Line[i].listen <= 0) Line.status = 1;
        }
    timer0(); 
    reload();
}//T0结束才会进行Reload

void check(){
    unsigned char i;
    for (i=0;i++;i<lineSize){
        if (Line[i].status == 4 && Line[i].listen == *(Line[i].pt)) Line[i].status = 1;
        if (Line[i].status == 5 && Line[i].listen < *(Line[i].pt)) Line[i].status = 1;
        if (Line[i].status == 6 && Line[i].listen > *(Line[i].pt)) Line[i].status = 1;
    }
}//检查一下内存之类的玩意是否满足条件，满足就转为就绪

void T1() interrupt 1{
    TR1=0;
    timer1(); 
    check();
}//T1检查内存，别忘了开