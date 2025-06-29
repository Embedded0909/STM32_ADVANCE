int main(void)
{

    ... SCB->VTOR = 0x0800C800;
    ... while (1)
    {

        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, 1);
        HAL_Delay(100);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, 0);
        HAL_Delay(100);
    }
}