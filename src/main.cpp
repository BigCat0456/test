#include <mbed.h>
#include "stm32f4xx_hal.h"
#include "stm32f429i_discovery_lcd.h"

extern "C" void wait_ms(int ms) { wait_us(1000*ms); } // without this the code fails

SPI spi(PF_9, PF_8, PF_7); 
DigitalOut cs(PC_1); 

//陀螺仪获取到的数据
struct GyroData{
    int16_t yaw; //偏航角：绕垂直于板面的z轴旋转，俯视时逆时针为正顺时针为负
    int16_t pitch; //俯仰角：绕平行于短边的轴旋转
    int16_t roll;  //滚转角：绕平行于长边的轴旋转 
};

class Screen {
public:
    Screen() {
        //Initializing the LCD screen
        HAL_Init();
        BSP_LCD_Init();
        BSP_LCD_LayerDefaultInit(0, LCD_FRAME_BUFFER);
        BSP_LCD_SelectLayer(0);
        BSP_LCD_DisplayOn(); //Turning display on
        BSP_LCD_Clear(LCD_COLOR_WHITE); //Clearing the screen
    }

    void displayText(const string& text, int line, uint32_t color) {
        BSP_LCD_SetTextColor(color); //Setting text color
        BSP_LCD_DisplayStringAt(0, LINE(line), (uint8_t*)text.c_str(), CENTER_MODE); //display text at center of the screen
    }

    void clear(){
        BSP_LCD_Clear(LCD_COLOR_WHITE); 
    }
};

class GyroMeter{
public:
    GyroMeter(SPI& spi, DigitalOut& cs) : spi(spi), cs(cs) {}

    //initialize gyrometer  
    void Init() { 
        cs = 0; 
        spi.write(0x20);
        spi.write(0xCF); 
        cs = 1; 
        spi.format(8, 3); 
        spi.frequency(100000);
    }   

    //extract the data from geometer
    GyroData GetData() {
        cs = 0; 
        spi.write(0xE8);
        GyroData result;

        int16_t data[6];
        for(int i = 0; i < 6; i++){
            data[i] = spi.write(0x00);
        }
        result.pitch = (int16_t)(data[1]<<8 | data[0]);
        result.roll = (int16_t)(data[3]<<8 | data[2]);
        result.yaw = (int16_t)(data[5]<<8 | data[4]);

        cs = 1;
        wait_us(50000);

        return result;
    }

    void reset_gyroscope() {
        cs = 0;                           // 拉低 Chip Select，开始 SPI 通信
        spi.write(0x20 | 0x40);           // 写入地址 (CTRL_REG5)，设置为写操作
        spi.write(0x80);                  // 设置 BOOT 位为 1
        cs = 1;                           // 拉高 Chip Select，结束 SPI 通信
        thread_sleep_for(10);             // 等待 10ms，让复位完成

        Init();
    }    

private:
    SPI& spi; //creating spi object for the gyroscope
    DigitalOut& cs; //creating digital out object to set chip select high or low

};


//按钮状态flag
volatile bool button_pressed = false;

//按钮按下事件
void buttonPressed() {
    button_pressed = true;
}

//按钮松开事件    
//**这两个按钮事件中有些行为可以执行有些事件会导致报错，不太知道为什么
//目前发现printf和设置led都会报错，遂只在此处修改flag在主循环中进行亮灯灭灯
void buttonReleased() {
    button_pressed = false;
}

int main() {

    //Create a gyrometer object
    //创建陀螺仪对象并初始化，可通过GetData()获取读数
    GyroMeter gyrometer(spi, cs); 
    Screen screen;

    // Set up button
    InterruptIn button(PA_0);

    BusOut led(LED1);

    // Set up button events  设置按钮监听事件
    button.rise(&buttonPressed);    // 当按钮被按下时触发
    button.fall(&buttonReleased);  // 当按钮被松开时触发

    int i = 0;
    gyrometer.reset_gyroscope();

    while (true) {

        //response of button events
        //此处按钮事件响应为按下亮灯松开灭灯，按需替换事件
        if(button_pressed) 
            led = 1;
        else 
            led = 0;

        //屏幕显示事件
        screen.clear();
        if(i > 13) i = 0;
        if(button_pressed) 
            screen.displayText("Hello world~", i, LCD_COLOR_BLACK);
        else 
            screen.displayText("Hello world!", i, LCD_COLOR_BLACK);
        i++;

        GyroData data = gyrometer.GetData();
        printf("%d, ", data.yaw);
        printf("%d, ", data.pitch);
        printf("%d\n", data.roll);

		fflush(stdout);
        thread_sleep_for(100);
    }
}
