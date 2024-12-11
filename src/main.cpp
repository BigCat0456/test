#include <mbed.h>

SPI spi(PF_9, PF_8, PF_7); 
DigitalOut cs(PC_1); 

//陀螺仪获取到的数据
struct GyroData{
    int16_t yaw; //偏航角：绕垂直于板面的z轴旋转，俯视时逆时针为正顺时针为负
    int16_t pitch; //俯仰角：绕平行于短边的轴旋转
    int16_t roll;  //滚转角：绕平行于长边的轴旋转 
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
        spi.frequency(400000);
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
        // write_register(L3GD20_CTRL_REG5, 0x80); // Reset the device
        // ThisThread::sleep_for(100ms);          // Wait for the reset to complete
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

    // Set up user button
    InterruptIn button(PA_0);

    BusOut led(LED1);

    //Set up button events  设置按钮监听事件
    button.rise(&buttonPressed);    // 当按钮被按下时触发
    button.fall(&buttonReleased);  // 当按钮被松开时触发

    cs = 0;
    spi.write(0x27 | 0x80); // Read bit is 0b10000000
    uint8_t whoami = spi.write(0x00); // Dummy write to read
    cs = 1;

    while (true) {
        printf("%d", whoami);
        //此处按钮按下亮灯松开灭灯，按需替换事件
        if(button_pressed) 
            led = 1;
        else 
            led = 0;

        GyroData data = gyrometer.GetData();
        printf("%d, ", data.yaw);
        printf("%d, ", data.pitch);
        printf("%d\n", data.roll);

		// printf("haha\n");
		// read_data(x, y, z);
		// printf("X: %d, Y: %d, Z:%d", x, y, z);
		fflush(stdout);
        thread_sleep_for(100);
    }
}