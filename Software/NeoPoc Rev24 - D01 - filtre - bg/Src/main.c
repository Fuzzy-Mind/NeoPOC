#include "main.h"
#include "fatfs.h"

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;
SPI_HandleTypeDef hspi1;
TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

int result1_array[300];
int result2_array[300];
int led_array_counter = 0;
extern int value1;
extern volatile int result1, result2;
int result1_old=0, result2_old=0;
int read_new1=0, read_new2=0;

int value2=0;
int value3=140;
int value4=180;

int first_value_of_result1=0;
int first_value_of_result2=0;
int first_value_flag=0;

FATFS fs;
FATFS *pfs;
FIL fil;
FRESULT fres;
DWORD fre_clust;
uint32_t total, free;
char sd_card_header[9]={'E', 'O', ' ', 'T', 'E', 'S', 'T', '\n'};

uint32_t ADC_BUF[4];								// ADC1 in 4 channeli icin buffer olusturuldu (ch1(PhotoDiode), ch2(LimitSwitch), ch3(Vbat), ch16(Temp)).
float temp_adc,vsense,temp_val;			// Dahili sicaklik sensoru okumasi icin kullanilan degiskenler.
char temp_val_str[20];							// Dahili sicaklik stringe cevrildikten sonra yazilicak buffer.
int Vbat=0;
int old_Vbat=0;
int icon_vbat=0;

extern uint8_t Rx_Byte[1];			//Gelen veri 					Daha önce stm32f1xx_it.c dosyasinda tanimladim oradan cek.
extern uint8_t Rx_Data[40];			//Gelen veri bufferi	Daha önce stm32f1xx_it.c dosyasinda tanimladim oradan cek.
extern uint8_t Rx_DataCnt;			//Gelen veri counteri	Daha önce stm32f1xx_it.c dosyasinda tanimladim oradan cek.
extern uint8_t Rx_Flag;					//Alma islemi flagi		Daha önce stm32f1xx_it.c dosyasinda tanimladim oradan cek.

char Tx_Buffer_Temperature[11]={0x84, 0x48, 0x0A, 0x82, 0x03, 0x00, 0x30, 0x30, 0x30, 0x30, 0x30};		//0300 VP'ye 00000 yaz
char Tx_Buffer_Change_Page[7]={0x84, 0x48, 0x04, 0x80, 0x03, 0x00, 0x01};  														// Go to page 1
char Tx_Buffer_Send_Data[8]={0x84, 0x48, 0x05, 0x82, 0x12, 0x01, 0x00, 0xA0};													// Send data to a variable point.
//char Tx_Buffer_Update_Date[13]={0x84, 0x48, 0x0A, 0x80, 0x1F, 0x5A, 0x19, 0x12, 0x19, 0x04, 0x09, 0x52, 0x01};
char Tx_Buffer_Get_Date[6]={0x84, 0x48, 0x03, 0x81, 0x20, 0x07};
char data_low, data_high;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_SPI1_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */

void sdcard_write(char data[])							// Write string to end of sd card file.
{
	if(f_mount(&fs, "", 0) != FR_OK)
    Error_Handler();
  
  /* eo.txt adinda dosya aç */
  if(f_open(&fil, "eo.txt", FA_OPEN_ALWAYS | FA_READ | FA_WRITE) != FR_OK)
		Error_Handler();
	
	/* Sona git */
	if(f_lseek(&fil, f_size(&fil))!= FR_OK)
		Error_Handler();
  
  /* Bos alani kontrol et */
  if(f_getfree("", &fre_clust, &pfs) != FR_OK)
    Error_Handler();
  
  total = (uint32_t)((pfs->n_fatent - 2) * pfs->csize * 0.5);
  free = (uint32_t)(fre_clust * pfs->csize * 0.5);   
    
  /* Eger 1 kb dan az ise hata ver */
  if(free < 1)
    Error_Handler();
  
  /* veri stringini yaz */
  f_printf(&fil, "%s", data);
	
  /* Dosyayi kapat */
  if(f_close(&fil) != FR_OK)
    Error_Handler();    
  
  /* Unmount SDCARD */
  if(f_mount(NULL, "", 1) != FR_OK)
    Error_Handler();
	
}

void sdcard_write_int(int data)							// Write string to end of sd card file.
{
	if(f_mount(&fs, "", 0) != FR_OK)
    Error_Handler();
  
  /* eo.txt adinda dosya aç */
  if(f_open(&fil, "eo.txt", FA_OPEN_ALWAYS | FA_READ | FA_WRITE) != FR_OK)
		Error_Handler();
	
	/* Sona git */
	if(f_lseek(&fil, f_size(&fil))!= FR_OK)
		Error_Handler();
  
  /* Bos alani kontrol et */
  if(f_getfree("", &fre_clust, &pfs) != FR_OK)
    Error_Handler();
  
  total = (uint32_t)((pfs->n_fatent - 2) * pfs->csize * 0.5);
  free = (uint32_t)(fre_clust * pfs->csize * 0.5);   
    
  /* Eger 1 kb dan az ise hata ver */
  if(free < 1)
    Error_Handler();
  
  /* veri stringini yaz */
  f_printf(&fil, "%d", data);
	
  /* Dosyayi kapat */
  if(f_close(&fil) != FR_OK)
    Error_Handler();    
  
  /* Unmount SDCARD */
  if(f_mount(NULL, "", 1) != FR_OK)
    Error_Handler();
	
}

void temp_val_calculate()										// Calculate internal temperature and send to screen.
{
	temp_adc = ADC_BUF[3];
	vsense=(temp_adc/4096)*3.3;
	temp_val=((1.43-vsense)*1000/4.3)+25; // Calculate internal temperature.
	sprintf(temp_val_str, "%.2f", temp_val);  // Float to string.
	for(int n=0; n<5; n++)
	{
		Tx_Buffer_Temperature[n+6]=temp_val_str[n];
	}
	HAL_UART_Transmit(&huart1, (uint8_t *)Tx_Buffer_Temperature, 11 , 100);
}

void change_page(char page)									// Change page on the screen.
{
	Tx_Buffer_Change_Page[6]=page;
	HAL_UART_Transmit(&huart1, (uint8_t *)Tx_Buffer_Change_Page, 7, 100);
}
void send_data_to_variable_point(char vp_high, char vp_low, int data_to_send)
{
	
	Tx_Buffer_Send_Data[4]=vp_high;
	Tx_Buffer_Send_Data[5]=vp_low;

	data_low = data_to_send;
	data_high = data_to_send>>8;
	
	Tx_Buffer_Send_Data[6] = data_high;			// write (0 t0 7) bits of data_to_send to.
	Tx_Buffer_Send_Data[7] = data_low;			// write (8 to 15) bits of data_to_send to data_high.
	
	HAL_UART_Transmit(&huart1, (uint8_t *)Tx_Buffer_Send_Data, 8, 10);

}
int main(void)
{
  HAL_Init();
  SystemClock_Config();
  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI1_Init();
  MX_FATFS_Init();
  MX_ADC1_Init();
  MX_TIM1_Init();
	MX_TIM2_Init();
  MX_TIM3_Init();
  MX_USART1_UART_Init();
  MX_USART3_UART_Init();
  
  /* USER CODE BEGIN 2 */
	
	HAL_ADC_Start_DMA(&hadc1, (uint32_t *)ADC_BUF, 4);  // ADC1'i DMA modda baslat. (ADC_BUF arraye 4 byte)
	HAL_ADC_Start_IT(&hadc1);													  // ADC1 için interrupt baslat.
	while(HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK);	// Kalibrasyon yap
	/**************************************************************************************/
	
	//HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1); Cahnnel 1 Arizali
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);		// Blue Led.
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);		// White Led.
	
	/**************************************************************************************/
	
	sdcard_write(sd_card_header);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	for(int j=0; j<9; j++)		// Animasyonu oynat.
	{
		HAL_Delay(120);
		change_page(j);
	}
	HAL_TIM_Base_Start_IT(&htim3);		// Buzzer On.
	HAL_Delay(100);
	change_page(10);									// Kartus takiniz sayfasina git.
	HAL_TIM_Base_Stop_IT(&htim3);			// Buzzer Off.
	HAL_TIM_Base_Stop_IT(&htim2);			// Ölçüm interruptini durdur.
	
  while (1)
  {	
		HAL_ADC_Start_IT(&hadc1);				// Update ADC
		temp_val_calculate();						// Calculate internal temperature and send to screen.
		
		Vbat=(ADC_BUF[2]-1431)/4.24;					// 1431 -> %0 batarya 1855 -> %100 batarya.
		Vbat=(Vbat+old_Vbat)/2;								// Son iki degerin aritmatik ortalamasini al. 1 birimlik oynamayi azaltmak icin.
		if(Vbat>100){Vbat=100;}
		if(Vbat<1){Vbat=0;}
		
		icon_vbat=Vbat/20;
		
		send_data_to_variable_point(0x03, 0x10, Vbat);
		send_data_to_variable_point(0x03, 0x10, Vbat);
		
		switch(icon_vbat)
		{
			case 0:
			{
				send_data_to_variable_point(0x00, 0x02, 0);
				break;
			}
			case 1: 
			{
				send_data_to_variable_point(0x00, 0x02, 1);
				break;
			}
			case 2: 
			{
				send_data_to_variable_point(0x00, 0x02, 2);
				break;
			}
			case 3: 
			{
				send_data_to_variable_point(0x00, 0x02, 3);
				break;
			}
			case 4: 
			{
				send_data_to_variable_point(0x00, 0x02, 4);
				break;
			}
			case 5: 
			{
				send_data_to_variable_point(0x00, 0x02, 4);
				break;
			}	
		}
		old_Vbat=Vbat;
		
		if(ADC_BUF[1]>500)							// Eger butona basildiysa.
		{	
			__HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);							// UART1 Rx Interrupt enabled.
			HAL_UART_Transmit(&huart1, (uint8_t*)Tx_Buffer_Get_Date, 6, 100); 	// Get Date
			HAL_Delay(30);																						// Wait until read Rx
			__HAL_UART_DISABLE_IT(&huart1, UART_IT_RXNE);							// Disable UART1 Rx interrupt.
			
			change_page(11);										// Kan damlat ekranina git.
			HAL_TIM_Base_Start_IT(&htim2);			// Ölçüm interruptini baslat.
			HAL_Delay(2000);										// 1 saniye sonra

			sdcard_write("---------- Measurements Results ----------\n");			// Ölçüme baslandigini gösteren basligi sd card'a yaz.
			value1=0;
			while(1)
			{
				if(value2+1==value1)
				{
					if(value1>50)
					{
						read_new1 = (result1_old*(value1-51)+result1)/(value1-50);
						result1_old=read_new1;
						read_new2 = (result2_old*(value1-51)+result2)/(value1-50);
						result2_old=read_new2;
					}

					value2=value1;
					if(value1==300)
					{
						HAL_TIM_Base_Stop_IT(&htim2);
						if(first_value_flag==0)
						{
							first_value_of_result1=read_new1;
							first_value_of_result2=read_new2;
							first_value_flag=1;
							send_data_to_variable_point(0x12, 0x01, first_value_of_result1);
							send_data_to_variable_point(0x12, 0x02, first_value_of_result2);
						}
						HAL_ADC_Start_IT(&hadc1);
						if(ADC_BUF[1]<=500)
						{
							HAL_TIM_Base_Stop_IT(&htim2);
							TIM1->CCR2=0;
							TIM2->CCR3=0;
							send_data_to_variable_point(0x11, 0x01, 0);
							send_data_to_variable_point(0x11, 0x02, 0);
							send_data_to_variable_point(0x12, 0x03, 0);
							send_data_to_variable_point(0x12, 0x04, 0);
							send_data_to_variable_point(0x12, 0x05, 140);
							result1_old=0;
							result2_old=0;
							value1=0;
							value2=0;
							value3=140;
							first_value_flag=0;
							change_page(10);
							break;
						}
						
						send_data_to_variable_point(0x11, 0x01, read_new1);
						send_data_to_variable_point(0x11, 0x02, read_new2);
						
						if(f_mount(&fs, "", 0) != FR_OK)
							Error_Handler();

						if(f_open(&fil, "eo.txt", FA_OPEN_ALWAYS | FA_READ | FA_WRITE) != FR_OK)
							Error_Handler();
						
						/* Sona git */
						if(f_lseek(&fil, f_size(&fil))!= FR_OK)
							Error_Handler();
						
						/* Bos alani kontrol et */
						if(f_getfree("", &fre_clust, &pfs) != FR_OK)
							Error_Handler();
						
						free = (uint32_t)(fre_clust * pfs->csize * 0.5);   
							
						/* Eger 1 kb dan az ise hata ver */
						if(free < 1)
							Error_Handler();
						
						/* veri stringini yaz */
						if(Rx_Flag==1)
						{
							f_printf(&fil, "%02x", Rx_Data[8]);		// Day
							f_printf(&fil, "%s", "-");		
							f_printf(&fil, "%02x", Rx_Data[7]);		// Month
							f_printf(&fil, "%s", "-");		
							f_printf(&fil, "20%02x", Rx_Data[6]);		// Year
							f_printf(&fil, "%s", " // ");		
							f_printf(&fil, "%02x", Rx_Data[10]);		// Hour
							f_printf(&fil, "%s", ":");						
							f_printf(&fil, "%02x", Rx_Data[11]);		// Minute
							f_printf(&fil, "%s", ":");
							f_printf(&fil, "%02x", Rx_Data[12]);		// Second
							f_printf(&fil, "%s", "  Vbat:");
							f_printf(&fil, "%d", ADC_BUF[2]);
							f_printf(&fil, "%s", "\n\n");
							Rx_Flag=0;
						}
						f_printf(&fil, "%d", read_new1);
						f_printf(&fil, "%s", ";");
						f_printf(&fil, "%d", read_new2);
						f_printf(&fil, "%s", ";");
						f_printf(&fil, "%d", 0);
						f_printf(&fil, "%s", "\n");
						
						/* Dosyayi kapat */
						if(f_close(&fil) != FR_OK)
							Error_Handler();    
						
						/* Unmount SDCARD */
						if(f_mount(NULL, "", 1) != FR_OK)
							Error_Handler();
						
						if((read_new2>(first_value_of_result2*1.1))||(read_new2<(first_value_of_result2*0.9)))	// +-%10 luk degisim olduysa.
						{
							read_new1=0;
							read_new2=0;
							result1_old=0;
							result2_old=0;
							value1=0;
							value2=0;
							change_page(12);
							HAL_TIM_Base_Start_IT(&htim2);
							while(1)
							{
								if(value2+1==value1)
								{
									if(value1>50)
									{
										read_new1 = (result1_old*(value1-51)+result1)/(value1-50);
										result1_old=read_new1;
										read_new2 = (result2_old*(value1-51)+result2)/(value1-50);
										result2_old=read_new2;
									}

									value2=value1;
									if(value1==300)
									{
										HAL_TIM_Base_Stop_IT(&htim2);
										HAL_ADC_Start_IT(&hadc1);
										if(ADC_BUF[1]<=500)
										{
											HAL_TIM_Base_Stop_IT(&htim2);
											TIM1->CCR2=0;
											TIM2->CCR3=0;
											value1=0;
											value2=0;
											result1_old=0;
											result2_old=0;
											first_value_flag=0;
											change_page(10);
											break;
										}
										
										send_data_to_variable_point(0x12, 0x03, read_new1);
										send_data_to_variable_point(0x12, 0x04, read_new2);
										send_data_to_variable_point(0x12, 0x05, value3);
										
										if(f_mount(&fs, "", 0) != FR_OK)
											Error_Handler();

										if(f_open(&fil, "eo.txt", FA_OPEN_ALWAYS | FA_READ | FA_WRITE) != FR_OK)
											Error_Handler();
										
										/* Sona git */
										if(f_lseek(&fil, f_size(&fil))!= FR_OK)
											Error_Handler();
										
										/* Bos alani kontrol et */
										if(f_getfree("", &fre_clust, &pfs) != FR_OK)
											Error_Handler();
										
										total = (uint32_t)((pfs->n_fatent - 2) * pfs->csize * 0.5);
										free = (uint32_t)(fre_clust * pfs->csize * 0.5);   
											
										/* Eger 1 kb dan az ise hata ver */
										if(free < 1)
											Error_Handler();
										
										/* veri stringini yaz */
										f_printf(&fil, "%d", read_new1);
										f_printf(&fil, "%s", ";");
										f_printf(&fil, "%d", read_new2);
										f_printf(&fil, "%s", ";");
										f_printf(&fil, "%d", value3);
										f_printf(&fil, "%s", "\n");
										
										/* Dosyayi kapat */
										if(f_close(&fil) != FR_OK)
											Error_Handler();    
										
										/* Unmount SDCARD */
										if(f_mount(NULL, "", 1) != FR_OK)
											Error_Handler();
										
										value3--;
										
										if(value3==0)
										{
											change_page(13);
											HAL_TIM_Base_Start_IT(&htim3);
											HAL_Delay(500);
											HAL_TIM_Base_Stop_IT(&htim3);
											//test
											HAL_TIM_Base_Start_IT(&htim2);
											value1=0;
											value2=0;
											while(1)
											{
												if(value2+1==value1)
												{
													if(value1>50)
													{
														read_new1 = (result1_old*(value1-51)+result1)/(value1-50);
														result1_old=read_new1;
														read_new2 = (result2_old*(value1-51)+result2)/(value1-50);
														result2_old=read_new2;
													}

													value2=value1;
													if(value1==300)
													{
														HAL_TIM_Base_Stop_IT(&htim2);
														
														if(f_mount(&fs, "", 0) != FR_OK)
															Error_Handler();

														if(f_open(&fil, "eo.txt", FA_OPEN_ALWAYS | FA_READ | FA_WRITE) != FR_OK)
															Error_Handler();
														
														/* Sona git */
														if(f_lseek(&fil, f_size(&fil))!= FR_OK)
															Error_Handler();
														
														/* Bos alani kontrol et */
														if(f_getfree("", &fre_clust, &pfs) != FR_OK)
															Error_Handler();
														
														total = (uint32_t)((pfs->n_fatent - 2) * pfs->csize * 0.5);
														free = (uint32_t)(fre_clust * pfs->csize * 0.5);   
															
														/* Eger 1 kb dan az ise hata ver */
														if(free < 1)
															Error_Handler();
														
														/* veri stringini yaz */
														f_printf(&fil, "%d", read_new1);
														f_printf(&fil, "%s", ";");
														f_printf(&fil, "%d", read_new2);
														f_printf(&fil, "%s", ";");
														f_printf(&fil, "%d", 0);
														f_printf(&fil, "%s", "\n");
														
														/* Dosyayi kapat */
														if(f_close(&fil) != FR_OK)
															Error_Handler();    
														
														/* Unmount SDCARD */
														if(f_mount(NULL, "", 1) != FR_OK)
															Error_Handler();
														
														value4--;
														if(value4==0)
														{
															while(1)
															{
																HAL_ADC_Start_IT(&hadc1);
																if(ADC_BUF[1]<=500)
																{
																	result1_old=0;
																	result2_old=0;
																	value3=140;
																	value4=180;
																	first_value_flag=0;
																	change_page(10);
																	break;
																}
															}
															break;
														}																		
														HAL_ADC_Start_IT(&hadc1);
														if(ADC_BUF[1]<=500)
														{
															result1_old=0;
															result2_old=0;
															value3=140;
															value4=180;
															change_page(10);
															break;
														}
														read_new1=0;
														read_new2=0;
														result1_old=0;
														result2_old=0;
														value1=0;
														value2=0;
														HAL_TIM_Base_Start_IT(&htim2);
													}
												}
											}
										}
										
										read_new1=0;
										read_new2=0;
										result1_old=0;
										result2_old=0;
										value1=0;
										value2=0;
										HAL_TIM_Base_Start_IT(&htim2);
									}
								}
							}
						}
						
						read_new1=0;
						read_new2=0;
						result1_old=0;
						result2_old=0;
						value1=0;
						value2=0;
						HAL_TIM_Base_Start_IT(&htim2);
					}
				}
			}
		}
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */
  /** Common config 
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 4;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel 
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel 
  */
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel 
  */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = ADC_REGULAR_RANK_3;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel 
  */
  sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
  sConfig.Rank = ADC_REGULAR_RANK_4;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 1;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 1000;  // 100
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 232;  // 232 800us icin
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 120;		// 120
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 18000;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 1;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}


static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 9600;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/** 
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void) 
{
  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, SD_CS_Pin|Buzzer_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LED_B_Pin|LED_G_Pin|LED_R_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : SD_CS_Pin Buzzer_Pin */
  GPIO_InitStruct.Pin = SD_CS_Pin|Buzzer_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : LED_B_Pin LED_G_Pin LED_R_Pin */
  GPIO_InitStruct.Pin = LED_B_Pin|LED_G_Pin|LED_R_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  while(1)
  {
		HAL_GPIO_TogglePin(LED_R_GPIO_Port, LED_R_Pin);
		HAL_Delay(100);
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
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
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
