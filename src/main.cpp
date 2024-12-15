#include <mbed.h>
#include "stm32f4xx_hal.h"
#include "stm32f429i_discovery_lcd.h"
#include <vector>

//compiling c together with cpp
extern "C" void wait_ms(int ms) { thread_sleep_for(500); }

SPI spi(PF_9, PF_8, PF_7); 
DigitalOut cs(PC_1); 

//data fetched from geometer
//陀螺仪获取到的数据
struct GyroData{
    int16_t yaw; //偏航角：绕垂直于板面的z轴旋转，俯视时逆时针为正顺时针为负
    int16_t pitch; //俯仰角：绕平行于短边的轴旋转
    int16_t roll;  //滚转角：绕平行于长边的轴旋转 
};

// 陀螺仪GyroData向量之间的对比
class GyroDataSequenceMathcer
{
private:
    double threshold; // this threshold is a value to determine the movement of stm32

//计算两个向量的distance
    double computeCrossCorrelation(const vector<int16_t>& x, const vector<int16_t>& y) {
        int n = min(x.size(), y.size());
        double distance = 0;
        int N = 0; 

        for (int i = 0; i < n; i++) {
            if (x[i] && y[i]) {
                distance += abs(x[i] - y[i]);
                N++;
            }
        }
        return N ? distance / N : 0;
    }
public:
    GyroDataSequenceMathcer(double threshold){
        this->threshold = threshold;
    };
    bool compare(const vector<GyroData>& seq1, const vector<GyroData>& seq2) {
        int n = min(seq1.size(), seq2.size()); 
        //minimum size of the recording and attempt sequences

        vector<int16_t> x1(n, 0), y1(n, 0), z1(n, 0); //constraining the size of each vector of measurements to the minimum between both sequences
        vector<int16_t> x2(n, 0), y2(n, 0), z2(n, 0); //constraining the size of each vector of measurements to the minimum between both sequences

        for (int i = 0; i < n; i++) {
            //populating the empty vectors with recording sequence measurements
            x1[i] = seq1[i].x;
            y1[i] = seq1[i].y;
            z1[i] = seq1[i].z;
            //populating the empty vectors with attempt sequence measurements
            x2[i] = seq2[i].x;
            y2[i] = seq2[i].y;
            z2[i] = seq2[i].z;
        }
        //computing cross correlation between x, y, z recording and attempt measurements
        // Determine level of relationship between both signals
        double distX = computeCrossCorrelation(x1, x2);
        double distY = computeCrossCorrelation(y1, y2);
        double distZ = computeCrossCorrelation(z1, z2);

        printf("Dist: %d, %d, %d\n", (int)distX, (int)distY, (int)distZ);
        // If cross correlation is under the threshold, the attempt sequence is correct
        bool correct = distX < threshold && distY < threshold && distZ < thr

class Screen {
public:
    //Initialize the screen
    Screen() {
        HAL_Init();
        BSP_LCD_Init();
        BSP_LCD_LayerDefaultInit(0, LCD_FRAME_BUFFER);
        BSP_LCD_SelectLayer(0);
        BSP_LCD_DisplayOn();
        BSP_LCD_Clear(LCD_COLOR_WHITE);
    }

    //show text on screen
    void displayText(const string& text, int line, uint32_t color) {
        BSP_LCD_SetTextColor(color); //Setting color
        BSP_LCD_DisplayStringAt(0, LINE(line), (uint8_t*)text.c_str(), CENTER_MODE); //display in center
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
        thread_sleep_for(50);

        return result;
    }

    //reset the gyroscope
    void reset_gyroscope() {
        cs = 0;                           
        spi.write(0x20 | 0x40);          
        spi.write(0x80);                 
        cs = 1;                           
        thread_sleep_for(10);            

        Init();
    }    

private:
    SPI& spi; //spi object for the gyroscope
    DigitalOut& cs; //digital out object to set chip select high or low

};

//button status flag
//按钮状态flag
volatile bool button_pressed = false;

//button event
//按钮按下事件
void buttonPressed() {
    button_pressed = true;
}

//button release event
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

	// recordData,记录状态下的单片机GyroData 状态 
	vector<GyroData> recordData; 
	// attemptData,测试状态下的单片机GyroData 状态
    	vector<GyroData> attemptData;

	// 本块思路参考https://github.com/chingun/ECE6483/blob/main/src/main.cpp ，后续把这注释删除
	
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
