/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

#include "main.h"
#include <stdio.h>
#include <string.h>

//Prototypes
void LCD_Init();
void Keypad_Init();
uint8_t Read_Keypad();

//LCD Prototypes
void LCD_nibble_write(uint8_t temp, uint8_t s);
void Write_String_LCD(char *temp);
void Write_Char_LCD(uint8_t code);
void Write_Instr_LCD(uint8_t code);
void Write_SR_LCD(uint8_t temp);

//7Seg Prototypes
void Write_SR_7S(uint8_t enable, uint8_t digit);
void Write_7Seg(uint8_t enable, uint8_t digit);
void Seven_Seg_Init(void);

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart5;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_UART5_Init();

//RFID ID Interpreter
uint8_t Receive_RFID_UID(uint8_t *uid);

//Transmit
void UART_Transmit_Message(char action, char *code);


//SEND EXIT
void UART_Transmit_EXIT(void);


int main(void)
{

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */

  /* Initialize all configured peripherals */
	
		HAL_Init();	
		MX_GPIO_Init();
		SystemClock_Config();
		MX_UART5_Init();

    Keypad_Init();
    LCD_Init();
	  Seven_Seg_Init();
		
		uint8_t key;
		uint8_t uid[8];
		char action;

		while (1)
		{
				if (Receive_RFID_UID(uid))
				{
						// Display the received UID
						//for (int i = 0; i < 8; i++)
						//{
							//	Write_Char_LCD(uid[i]);
					
					//Prompt user CHECK IN or RETURN
					Write_String_LCD("A to CHECK OUT");
					Write_Instr_LCD(0xC0);
					Write_String_LCD("B to RETURN");
					
					//Wait for Input
					key = Read_Keypad();
					
					if(key == 10) 
						action = 'A';
					
					else if(key == 11) 
						action = 'B';
					
					else {
						//SEND EXIT\n to RPP
						UART_Transmit_EXIT();
						continue;
					}	
					
					Write_Instr_LCD(0x01);		//Clear LCD
					Write_String_LCD("Enter Part #");
					Write_Instr_LCD(0xC0);
					
					char code[4] = {};
					for(int i = 0; i < 4; i++) 
					{
						key = Read_Keypad();
					//	if(key != 10 | key != 11) 
						//	UART_Transmit_EXIT();
						code[i] = key + '0';	//Convert key to char
						Write_Char_LCD(key+'0');

					}
					
					HAL_Delay(2000);
					
					Write_Instr_LCD(0x01);		//Clear LCD
					
					//Send packet_tx to RPP
					UART_Transmit_Message(action, code);
					
					
				}				
			}			
}



void UART_Transmit_EXIT(void)
{
    uint8_t message[] = "EXIT\n";

    HAL_UART_Transmit(&huart5,
                      message,
                      sizeof(message) - 1,
                      HAL_MAX_DELAY);
}

//RECEIVE DATA PACKAGE
uint8_t Receive_RFID_UID(uint8_t *uid)
{
    uint8_t rxData[14];

    if (HAL_UART_Receive(&huart5, rxData, 14, 1000) == HAL_OK)
    {
        // Verify message identifier
        if (rxData[0] == 'S' &&
            rxData[1] == 'C' &&
            rxData[2] == 'A' &&
            rxData[3] == 'N' &&
            rxData[4] == ',')
        {
            // Extract the 8 UID characters
            for (int i = 0; i < 8; i++)
            {
                uid[i] = rxData[i + 5];
            }

            return 1;  // Successful reception
        }
    }

    return 0;  // Invalid or incomplete message
}

//TRANSMIT DATA PACKAGE
void UART_Transmit_Message(char action, char *code)
{
    char txData[14] = "ERROR\n";

    if (action == 'A')
    {
			txData[0] = 'C';
			txData[1] = 'H';
			txData[2] = 'E';
			txData[3] = 'C';
			txData[4] = 'K';
			txData[5] = 'O';
			txData[6] = 'U';
			txData[7] = 'T';
			txData[8] = ',';
			txData[9] = code[0];
			txData[10] = code[1];
			txData[11] = code[2];
			txData[12] = code[3];
			txData[13] = '\n';
    }
    else if (action == 'B')
    {
				txData[0] = 'R';
				txData[1] = 'E';
				txData[2] = 'T';
				txData[3] = 'U';
				txData[4] = 'R';
				txData[5] = 'N';
				txData[6] = ',';
				txData[7] = code[0];
				txData[8] = code[1];
				txData[9] = code[2];
				txData[10] = code[3];
				txData[11] = '\n';
    }
		
    HAL_UART_Transmit(&huart5,(uint8_t *)txData, strlen(txData), HAL_MAX_DELAY);
}


// USUAL FUNCTIONS*********************************************************************************************

uint8_t Read_Keypad()
{

/* All colomns are zeros col0  PA0  - col1  PA9 - col2 PA10   col3  PB4*/	

	uint8_t a;
	/*set all columns high and wait until a putton is pressed*/ 
	GPIOB->ODR|=(1<<1);
	GPIOB->ODR|=(1<<2); 
	GPIOB->ODR|=(1<<3); 
	GPIOB->ODR|=(1<<4);

	while((GPIOB->IDR &(0x1<<8))==0 && (GPIOB->IDR &(0x1<<9))==0 && (GPIOB->IDR &(0x1<<10))==0 &&	(GPIOB->IDR &(0x1<<11))==0)
	{

	}

	HAL_Delay(25);	/*debouncing*/
/*scanning */ 
	while(1){
	GPIOB->ODR&=~(1<<1); 
	GPIOB->ODR&=~(1<<2); 
	GPIOB->ODR&=~(1<<3); 
	GPIOB->ODR&=~(1<<4);
	
	/* Scan Col 0 – PB1 = high*/
	/* check rows */

	GPIOB->ODR|=(1<<1);
	HAL_Delay(2);

	
	/* check rows*/
		
	if((GPIOB->IDR &(0x1<<8))!=0)
		{a=1;
		break;
	}
			if((GPIOB->IDR &(0x1<<9))!=0) 
		{a=4;
	break;}
		 	if((GPIOB->IDR &(0x1<<10))!=0)
	 {
		a=7;
		break;
	 }
	 
	if((GPIOB->IDR &(0x1<<11))!=0)
	{
		a=14;
		break;
		}


	/* Scan Col 1 */ 

	GPIOB->ODR&=~(1<<1);
	HAL_Delay(2);		
	GPIOB->ODR|=(1<<2);
	HAL_Delay(2);	
	if((GPIOB->IDR &(0x1<<8))!=0)
		{
			a=2;
			break;
		}
	if((GPIOB->IDR &(0x1<<9))!=0) 
		{
			a=5;
			break;
		}
	if((GPIOB->IDR &(0x1<<10))!=0)
		{
			a=8;
			break;
		}
	if((GPIOB->IDR &(0x1<<11))!=0)
		{
			a=0;
			break;
		}


	
	/* Scan Col 2 */ 
	GPIOB->ODR&=~(1<<2);
	HAL_Delay(2);
	GPIOB->ODR|=(1<<3);
	HAL_Delay(2);
	if((GPIOB->IDR &(0x1<<8))!=0)
	{a=3;
	break;}
	if((GPIOB->IDR &(0x1<<9))!=0)
	{a=6;
	break;}
	if((GPIOB->IDR &(0x1<<10))!=0)
	{a=9;
	break;}
	if((GPIOB->IDR &(0x1<<11))!=0)
	{a=15;
	break;}

	/* Scan Col 3 */ 
	GPIOB->ODR&=~(1<<3);
	HAL_Delay(2);
	GPIOB->ODR|=(1<<4);
	HAL_Delay(2);

	if((GPIOB->IDR &(0x1<<8))!=0)
	{a=10;
	break;}
	if((GPIOB->IDR &(0x1<<9))!=0)
	{a=11;
	break;}
	if((GPIOB->IDR &(0x1<<10))!=0)
	{a=12;
	break;}
	if((GPIOB->IDR &(0x1<<11))!=0)
	{a=13;
	break;}

	}
	/*wait until button is released*/ 
	GPIOB->ODR|=(1<<1);
	HAL_Delay(2);
	GPIOB->ODR|=(1<<2);
	HAL_Delay(2);
	GPIOB->ODR|=(1<<3);
	HAL_Delay(2);
	GPIOB->ODR|=(1<<4);
	HAL_Delay(2);

	while(!(	((GPIOB->IDR &(0x1<<8))==0) && ((GPIOB->IDR &(0x1<<9))==0) && 	((GPIOB->IDR &(0x1<<10))==0)	&&	((GPIOB->IDR &(0x1<<11))==0) ))
	{}
	HAL_Delay(25); 
	return(a);
}

void Keypad_Init()
	{
	uint32_t temp;
	/* enable GPIOB clock */ 
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN; 

	/*configure input*/
	/* row0 to 3 are PB11, PB10, PB9, PB8 */ 
	temp = GPIOB->MODER;
	temp &= ~(0x03<<(2*11)); 
	temp &= ~(0x03<<(2*10)); temp &= ~(0x03<<(2*9)); 
	temp &= ~(0x03<<(2*8)); 
	GPIOB->MODER = temp;
	temp=GPIOB->OTYPER;
	temp &=~(0x01<<11); 
	temp &=~(0x01<<10); 
	temp &=~(0x01<<9); 
	temp &=~(0x01<<8); 
	GPIOB->OTYPER=temp;

	temp=GPIOB->PUPDR;
	temp&=~(0x03<<(2*11)); 
	temp&=~(0x03<<(2*10)); 
	temp&=~(0x03<<(2*9)); 
	temp&=~(0x03<<(2*8)); 
	GPIOB->PUPDR=temp;

	/* Col 0 to 3 are PB1, PB2, PB3, PB4*/
	/*configure output*/ 
	temp = GPIOB->MODER;
	temp &= ~(0x03<<(2*1)); 
	temp|=(0x01<<(2*1)); 
	temp &= ~(0x03<<(2*2)); 
	temp|=(0x01<<(2*2)); 
	temp &= ~(0x03<<(2*3)); 
	temp|=(0x01<<(2*3));
	temp &= ~(0x03<<(2*4)); 
	temp|=(0x01<<(2*4));
	GPIOB->MODER = temp;

	temp=GPIOB->OTYPER;
	temp &=~(0x01<<1); 
	temp &=~(0x01<<2); 
	temp &=~(0x01<<3); 
	temp &=~(0x01<<4); 
	GPIOB->OTYPER=temp;

	temp=GPIOB->PUPDR;
	temp&=~(0x03<<(2*1));
	temp&=~(0x03<<(2*2)); 
	temp&=~(0x03<<(2*3)); 
	temp&=~(0x03<<(2*4));
	GPIOB->PUPDR=temp;

}
void LCD_Init()
	{
		uint32_t temp;
		
        /* enable GPIOA clock */ 
        RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN; 
        /* enable GPIOB clock */ 
        RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
        /*PB5 MOSI, PA10 /CS_7 latch , PA5 shift clock */

			 
        /*PA5 and PA10 are outputs*/ temp = GPIOA->MODER;
        temp &= ~(0x03<<(2*5)); temp|=(0x01<<(2*5)); 
        temp &= ~(0x03<<(2*10)); temp|=(0x01<<(2*10)); 
        GPIOA->MODER = temp;
			 
        temp=GPIOA->OTYPER;
        temp &=~(0x01<<5);
        temp &=~(0x01<<10); GPIOA->OTYPER=temp;

        temp=GPIOA->PUPDR;
        temp&=~(0x03<<(2*5));
        temp&=~(0x03<<(2*10)); GPIOA->PUPDR=temp;

        /*PB5 is output*/

        temp = GPIOB->MODER;
        temp &= ~(0x03<<(2*5)); 
        temp|=(0x01<<(2*5)); 
        GPIOB->MODER = temp;

			 
        temp=GPIOB->OTYPER;
        temp &=~(0x01<<5); 
        GPIOB->OTYPER=temp;

            
        temp=GPIOB->PUPDR;
        temp&=~(0x03<<(2*5)); 
        GPIOB->PUPDR=temp;

		
		
        /* LCD controller reset sequence */ 
        HAL_Delay(20);
        LCD_nibble_write(0x30,0); 
        HAL_Delay(5); 
        LCD_nibble_write(0x30,0); 
        HAL_Delay(1); 
        LCD_nibble_write(0x30,0);
        HAL_Delay(1); 
        LCD_nibble_write(0x20,0); 
        HAL_Delay(1);

        Write_Instr_LCD(0x28); /* set 4 bit data LCD - two line display - 5x8 font*/ 
        Write_Instr_LCD(0x0E); /* turn on display, turn on cursor , turn off blinking */ 
        Write_Instr_LCD(0x01); /* clear display screen and return to home position*/ 
        Write_Instr_LCD(0x06); /* move cursor to right (entry mode set instruction)*/


}

	
	
void LCD_nibble_write(uint8_t temp, uint8_t s){

/*writing instruction*/ 
if (s==0){ 
	temp=temp&0xF0;
	temp=temp|0x02; /*RS (bit 0) = 0 for Command EN (bit1)=high */ 
	Write_SR_LCD(temp);

    temp=temp&0xFD; /*RS (bit 0) = 0 for Command EN (bit1) = low*/ 
    Write_SR_LCD(temp);	}

/*writing data*/ 
else if (s==1) {
	temp=temp&0xF0;
  temp=temp|0x03;	/*RS(bit 0)=1 for data EN (bit1) = high*/ 
  Write_SR_LCD(temp);

  temp=temp&0xFD; /*RS(bit 0)=1 for data EN(bit1) = low*/ 
  Write_SR_LCD(temp); 
}}

	
	

void Write_String_LCD(char *temp) 
{ 
    int i=0;

    while(temp[i]!=0)
        {
            Write_Char_LCD(temp[i]); i=i+1;
        }
}




void Write_Instr_LCD(uint8_t code)
{
LCD_nibble_write(code&0xF0,0);

code=code<<4; LCD_nibble_write(code,0);
}


void Write_Char_LCD(uint8_t code)
{
    LCD_nibble_write(code&0xF0,1);

    code=code<<4;
    LCD_nibble_write(code,1);
}


void Write_SR_LCD(uint8_t temp)
{
int i;
uint8_t mask=0b10000000;
	
for(i=0; i<8; i++) {
        if((temp&mask)==0)
        GPIOB->ODR&=~(1<<5);
        else
        GPIOB->ODR|=(1<<5);

        /*	Sclck */
        GPIOA->ODR&=~(1<<5); GPIOA->ODR|=(1<<5);
        HAL_Delay(1);

        mask=mask>>1;
        }

    /*Latch*/
    GPIOA->ODR|=(1<<10); 
    GPIOA->ODR&=~(1<<10);
}
	
//7Seg Functions

void Seven_Seg_Init(void)
{
    uint32_t temp;

    /* Enable GPIO clocks */
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN;

    /* PA5 and PC10 as outputs */
    temp = GPIOA->MODER;
    temp &= ~(0x03 << (2*5));
    temp |=  (0x01 << (2*5));
    GPIOA->MODER = temp;

    temp = GPIOC->MODER;
    temp &= ~(0x03 << (2*10));
    temp |=  (0x01 << (2*10));
    GPIOC->MODER = temp;

    /* PB5 as output */
    temp = GPIOB->MODER;
    temp &= ~(0x03 << (2*5));
    temp |=  (0x01 << (2*5));
    GPIOB->MODER = temp;

    /* Push-pull output configuration */
    GPIOA->OTYPER &= ~(1 << 5);
    GPIOC->OTYPER &= ~(1 << 10);
    GPIOB->OTYPER &= ~(1 << 5);

    /* No pull-up or pull-down */
    GPIOA->PUPDR &= ~(0x03 << (2*5));
    GPIOC->PUPDR &= ~(0x03 << (2*10));
    GPIOB->PUPDR &= ~(0x03 << (2*5));

    /* Initialize outputs low */
    GPIOA->ODR &= ~(1 << 5);
    GPIOB->ODR &= ~(1 << 5);
    GPIOC->ODR &= ~(1 << 10);
}

void Write_SR_7S(uint8_t enable, uint8_t digit) {
		uint8_t mask = 0b10000000;
			for (int i=0; i<8; i++) {
			if ((digit & mask)==0) GPIOB->ODR &= ~(1<<5); /* data = 0 */
			else GPIOB->ODR |= (1<<5); /* data = 1 */
			GPIOA->ODR &= ~(1<<5); HAL_Delay(1); /* SCLK low */
			GPIOA->ODR |= (1<<5); HAL_Delay(1); /* SCLK high — shift! */
			mask = mask >> 1;
			}
		/* ... repeat the same loop for the "enable" byte ... */
			for (int i=0; i<8; i++) {
			if ((enable & mask)==0) GPIOC->ODR &= ~(1<<10); /* data = 0 */
			else GPIOC->ODR |= (1<<10); /* data = 1 */
			GPIOA->ODR &= ~(1<<5); HAL_Delay(1); /* SCLK low */
			GPIOA->ODR |= (1<<5); HAL_Delay(1); /* SCLK high — shift! */
			mask = mask >> 1;
			}
		/* Latch: copy shifted bits to the outputs */
		GPIOC->ODR |= (1<<10);
		GPIOC->ODR &= ~(1<<10);
		}

void Write_7Seg(uint8_t enable, uint8_t digit) {
		uint8_t Enable[5] = {0x00, 0x08, 0x04, 0x02, 0x01};
		/* one-hot per display */
		uint8_t Digit[10] = {0xC0,0xF9,0xA4,0xB0,0x99,
		0x92,0x82,0xF8,0x80,0x90}; /* 0-9 segment codes */
		Write_SR_7S(Enable[enable], Digit[digit]);
		}




/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief UART5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART5_Init(void)
{

  /* USER CODE BEGIN UART5_Init 0 */

  /* USER CODE END UART5_Init 0 */

  /* USER CODE BEGIN UART5_Init 1 */

  /* USER CODE END UART5_Init 1 */
  huart5.Instance = UART5;
  huart5.Init.BaudRate = 115200;
  huart5.Init.WordLength = UART_WORDLENGTH_8B;
  huart5.Init.StopBits = UART_STOPBITS_1;
  huart5.Init.Parity = UART_PARITY_NONE;
  huart5.Init.Mode = UART_MODE_TX_RX;
  huart5.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart5.Init.OverSampling = UART_OVERSAMPLING_16;
  huart5.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart5.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart5) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART5_Init 2 */

  /* USER CODE END UART5_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
