#include <egt/ui>
#include <egt/uiloader.h>
#include <iostream>

#define ENABLE_UART

#ifdef ENABLE_UART
#include "uartFunc.h"

#define STR_BLE_NOTY_INCOMING_CALL		"You have an incoming call."
#define STR_BLE_NOTY_REMOVE_CALL 		"An incoming call has been removed."
#define STR_BLE_NOTY_RETRIVE_DETAIL		"BLE_ANCS_EVT_NTFY_ATTR_IND."
#define STR_BLE_NOTY_ANSWER_CALL		"Answer the incoming call:"
#define STR_BLE_NOTY_RETRIEVE_SOCIAL	"want to retrieve a social message"
#define STR_BLE_NOTY_MISSED_CALL		"You have a missed call"

typedef enum __BLE_STATE_MACHONE__
{
	BLE_NOTY_NONE = 0,
	BLE_NOTY_INCOMING_CALL,		// new call coming
	BLE_NOTY_REMOVE_CALL,		// call was cancelled by caller
	BLE_NOTY_RETRIEVE_DETAIL,	// retrieve caller detail
	BLE_NOTY_ANSWER_CALL,		// answer call
	BLE_NOTY_SOCIAL_MEDIA,		// retrieve social media
	BLE_NOTY_MISSED_CALL,		// missed call
} BLE_NOTIFICATION;

typedef struct __BLE_CALLER_INFO__
{
	char caller_number[32];
	char msg[512];
	char date[32];
} BLE_CALLER_INFO, *pBLE_CALLER_INFO;

#define RECEIVE_BUFFER_SIZE	1024

void debug_buffer(char *buf, int len)
{
	for(int i=0; i<len; i++)
	{
		printf("%02X ", buf[i]);
	}
	
	printf("\n\r");
	
	for(int i=0; i<len; i++)
	{
		printf("%c", buf[i]);
	}
	
	printf("\n\r");

}

int ble_get_caller_detail(char *buf, int length, pBLE_CALLER_INFO caller_info)
{
	int i;
	int last_line_pos = -1;
	int index = 0;
	
	for(i=0; i<length; i++)
	{
		if( buf[i] == 0x0A )
		{
			switch( index )
			{
				case 0:	// BLE_ANCS_EVT_NTFY_ATTR_IND
					break;
				
				case 1:	// title
					memcpy(caller_info->caller_number, buf+(last_line_pos+1+9), i-last_line_pos-1-9);
					caller_info->caller_number[i-last_line_pos-1-9] = 0;
					break;
					
				case 2:	// sybtitle
					break;
					
				case 3:	// msg
					memcpy(caller_info->msg, buf+(last_line_pos+1+7), i-last_line_pos-1-7);
					caller_info->msg[i-last_line_pos-1-7] = 0;
					break;
					
				case 4:	// date
					memcpy(caller_info->date, buf+(last_line_pos+1+8), i-last_line_pos-1-8);
					caller_info->date[i-last_line_pos-1-8] = 0;
					break;
					
				default:
					break;
			}
		
			last_line_pos = i;	
			index++;
		}
	}
	
	return 0;
}

BLE_NOTIFICATION ble_identity_notification(char *buf, int length)
{
	int index;
	
	if( strstr(buf, STR_BLE_NOTY_INCOMING_CALL) != NULL )
	{
		return BLE_NOTY_INCOMING_CALL;
	}
	else if( strstr(buf, STR_BLE_NOTY_REMOVE_CALL) != NULL )
	{
		return BLE_NOTY_REMOVE_CALL;
	}
	else if( strstr(buf, STR_BLE_NOTY_RETRIVE_DETAIL) != NULL )
	{
		return BLE_NOTY_RETRIEVE_DETAIL;
	}
	else if( strstr(buf, STR_BLE_NOTY_ANSWER_CALL) != NULL )
	{
		return BLE_NOTY_ANSWER_CALL;
	}
	else if( strstr(buf, STR_BLE_NOTY_RETRIEVE_SOCIAL) != NULL )
	{
		return BLE_NOTY_SOCIAL_MEDIA;
	}
	else if( strstr(buf, STR_BLE_NOTY_MISSED_CALL) != NULL )
	{
		return BLE_NOTY_MISSED_CALL;
	}
	
	return BLE_NOTY_NONE;
}

char recBuff[RECEIVE_BUFFER_SIZE];
char cmdBuff[RECEIVE_BUFFER_SIZE];	

#endif // end of ENABLE_UART

#define WORKING_TIMER		10
#define REFRESH_PERIOD		(500/WORKING_TIMER)

int main(int argc, char** argv)
{
	bool isCalling = false;
	bool isAnswered = false;
	bool isSMS = false;
	bool isReject = false;
	int refresh_count = 0;
	
	// emulate speed
	int count = 0;
	int testSpeed = 0;

#ifdef ENABLE_UART
	struct pollfd pollUartfds;
	int nread;
	
	BLE_NOTIFICATION ble_no;
	BLE_CALLER_INFO caller_info;
	
	if( argc != 2 )
	{
		std::cout << "USAGE: uart_transmit UART_PORT" << std::endl;
		return -1;
	}
	
	int fdUart;
	fdUart = uartOpen(argv[1]);
	uartSetSpeed(fdUart, 115200);

	if (uartSetParity(fdUart,8,1,'N') == -1)
	{
		printf("Set Parity Error\n");
		return -1;
	}
	else
	{
		printf("%s connected\r\n", argv[1]);
	}
	
	pollUartfds.fd = fdUart;
	pollUartfds.events = POLLRDNORM;
#endif	// end of ENABLE_UART
	
	egt::Application app(argc, argv);
	egt::experimental::UiLoader loader;
	
	auto window = std::static_pointer_cast<egt::Window>(loader.load("file:ui.xml"));

	auto iconCall = window->find_child<egt::ImageLabel>("imgCalling");
	auto iconSMS = window->find_child<egt::ImageLabel>("imgSMS");
	auto btnAccept = window->find_child<egt::ImageButton>("btnAccept");
	auto btnReject = window->find_child<egt::ImageButton>("btnReject");
	auto labelCaller = window->find_child<egt::Label>("labCaller");
	auto labelMessage = window->find_child<egt::Label>("labelMessage");
	auto iconBLE = window->find_child<egt::Label>("iconBLE");
	auto labSpeed1 = window->find_child<egt::Label>("labSpeed1");
	auto labSpeed2 = window->find_child<egt::Label>("labSpeed2");
	auto labSpeed3 = window->find_child<egt::Label>("LabSpeed3");
	
	iconCall->hide();
	iconSMS->hide();
	btnAccept->hide();
	btnReject->hide();
	labelCaller->hide();
	labelMessage->hide();
	iconBLE->hide();
	labSpeed1->hide();
	labSpeed2->hide();
	labSpeed3->hide();
		
	window->show();
	
	btnAccept->on_click([&](egt::Event&)
	{
		if( isCalling )
		{
			std::cout << "Accpet..." << std::endl;
#ifdef ENABLE_UART			
			strcpy(cmdBuff, "Y\n\r");
			write(fdUart, cmdBuff, 3);
			
			isAnswered = true;
			isCalling = false;
			btnAccept->hide();
			btnReject->show();
			iconCall->show();
			labelCaller->show();
#endif	// end of ENABLE_UART
		}
	});
	
	btnReject->on_click([&](egt::Event&)
	{
		if( isCalling || isAnswered)
		{
			std::cout << "Reject..." << std::endl;
#ifdef ENABLE_UART			
			strcpy(cmdBuff, "N\n\r");
			write(fdUart, cmdBuff, 3);
			
			isReject = true;
#endif	// end of ENABLE_UART			
		}
	});
	
	egt::PeriodicTimer timer(std::chrono::milliseconds(WORKING_TIMER));
	timer.on_timeout([&]()
    {
#ifdef ENABLE_UART    
		if( 0 < poll(&pollUartfds, 1, 0) )
		{
			// check if any data came from UART
			if( (nread = read(fdUart, recBuff, 512)) >0)
			{
				debug_buffer(recBuff, nread);
				
				ble_no = ble_identity_notification(recBuff, nread);
				switch( ble_no )
				{
					case BLE_NOTY_INCOMING_CALL:
						strcpy(cmdBuff, "Y\n\r");
						write(fdUart, cmdBuff, 3);
						break;
						
					case BLE_NOTY_REMOVE_CALL:
						isCalling = false;
						
						iconCall->hide();
						btnAccept->hide();
						btnReject->hide();
						labelCaller->hide();
						break;
					
					case BLE_NOTY_RETRIEVE_DETAIL:
						ble_get_caller_detail(recBuff, nread, &caller_info);
						printf("Caller: %s\r\n", caller_info.caller_number);
						printf("MSG: %s\r\n", caller_info.msg);
						printf("Date: %s\r\n", caller_info.date);
						
						strcpy(cmdBuff, "Y\n\r");
						write(fdUart, cmdBuff, 3);
						break;
						
					case BLE_NOTY_ANSWER_CALL:
						isCalling = true;
						break;
						
					case BLE_NOTY_SOCIAL_MEDIA:
						if( !isReject )
						{
							printf("BLE_NOTY_SOCIAL_MEDIA\n\r");
							strcpy(cmdBuff, "Y\n\r");
							write(fdUart, cmdBuff, 3);
							isSMS = true;
						}
						else
						{
							isReject = false;
						}
						break;
						
					case BLE_NOTY_MISSED_CALL:
						isCalling = false;
						
						iconCall->hide();
						btnAccept->hide();
						btnReject->hide();
						labelCaller->hide();
						break;
						
					default:
						break;
				}
			
				memset(recBuff, 0, RECEIVE_BUFFER_SIZE);
			}
    	}
#endif // end of ENABLE_UART    	

    	if( isCalling )
    	{
#ifdef ENABLE_UART	    	
    		labelCaller->text(caller_info.caller_number);
#endif    		
    		labelCaller->show();
    		
    		btnAccept->show();
    		btnReject->show();
    		iconBLE->show();
    		
    		if( REFRESH_PERIOD <= refresh_count )
    		{
				if( iconCall->visible() )
				{
					iconCall->hide();
				}
				else
				{
					iconCall->show();
				}
				refresh_count = 0;
			}
			
			refresh_count++;
    	}
    	
    	if( isSMS )
    	{
#ifdef ENABLE_UART	    	
			labelCaller->text(caller_info.caller_number);
    		labelMessage->text(caller_info.msg);
#endif    		
    		labelCaller->show();  
    		labelMessage->show();
    		  	
    		if( (refresh_count%REFRESH_PERIOD == 0) && refresh_count>100)
    		{
				if( iconSMS->visible() )
				{
					iconSMS->hide();
				}
				else
				{
					iconSMS->show();
				}
			}
			
			refresh_count++;
			
			if( refresh_count >= REFRESH_PERIOD * 12 )
			{
				isSMS = false;
				iconSMS->hide();
				labelCaller->hide();
				labelMessage->hide();
				
				refresh_count = 0;
				labelCaller->text("");
				labelMessage->text("");
			}
    	}
		
		if( (count % 10) == 0 )
		{
			int dig1, dig2, dig3;
			dig1 = testSpeed%10;
			dig2 = (testSpeed%100)/10;
			dig3 = (testSpeed%1000)/100;
			
			labSpeed1->text(std::to_string(dig1));
			labSpeed1->show();
			if( dig3 != 0 || dig2 != 0 )
			{
				labSpeed2->text(std::to_string(dig2));
				labSpeed2->show();
			}
			else
			{
				labSpeed2->hide();
			}
			if( dig3 != 0 )
			{
				labSpeed3->text(std::to_string(dig3));
				labSpeed3->show();
			}
			else
			{
				labSpeed3->hide();
			}
			
			testSpeed++;
			if( testSpeed >= 200 )
			{
				testSpeed = 0;
			}
    	}
    	
		count++;
    });
    timer.start();

    return app.run();
}

