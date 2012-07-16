/*******************************************************************
    Copyright (C) 2009 FreakLabs
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. Neither the name of the the copyright holder nor the names of its contributors
       may be used to endorse or promote products derived from this software
       without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
    OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.

    Originally written by Christopher Wang aka Akiba.
    Please post support questions to the FreakLabs forum.
*******************************************************************/

/**************************************************************************/
/*! 
    @file     cmd.c
    @author   Christopher Wang (Freaklabs)
              Modified by: K. Townsend (microBuilder.eu)
    @date     19 May 2010

    Original code taken from the FreakUSB Open Source USB Device Stack
    http://freaklabs.org/index.php/FreakUSB-Open-Source-USB-Device-Stack.html

    If it works well, you can thank Akiba at Freaklabs.  If it fails
    miserably, you can blame me (since parts of it it were rather
    ungraciously modified). :-)

*/
/**************************************************************************/

#include <stdio.h>
#include <string.h>

#include "cmd.h"
#include "project/cmd_tbl.h"
#include "sysdefs.h"

#if CFG_INTERFACE_ENABLEIRQ == 1
  #include "core/gpio/gpio.h"
#endif

static uint8_t msg[CFG_INTERFACE_MAXMSGSIZE];
static uint8_t *msg_ptr;

/**************************************************************************/
/*! 
    @brief  Polls the relevant incoming message queue to see if anything
            is waiting to be processed.
*/
/**************************************************************************/
void cmdPoll()
{
  int c;
  while (EOF != (c = pf_getchar()))
    cmdRx(c);
}

/**************************************************************************/
/*! 
    @brief  Handles a single incoming character.  If a new line is 
            detected, the entire command will be passed to the command
            parser.  If a text character is detected, it will be added to
            the message buffer until a new line is detected (up to the
            maximum queue size, CFG_INTERFACE_MAXMSGSIZE).

    @param[in]  c
                The character to parse.
*/
/**************************************************************************/
uint8_t cmdRx(uint8_t c)
{
  // read out the data in the buffer and echo it back to the host. 
  switch (c)
  {
    case '\r':
      #if CFG_INTERFACE_DROPCR == 1
      break;
      #endif
    case '\n':
        // terminate the msg and reset the msg ptr. then send
        // it to the handler for processing.
        *msg_ptr = '\0';
        #if CFG_INTERFACE_SILENTMODE == 0
        printf("%s", CFG_PRINTF_NEWLINE);
        #endif
        cmdParse((char *)msg);
        msg_ptr = msg;
        return 1;
    
    case '\b':
        #if CFG_INTERFACE_SILENTMODE == 0
        printf("%c",c);
        #endif
        if (msg_ptr == msg)
        {
            // Send bell alert and space (to maintain position)
            printf("\a ");
        }
        else if (msg_ptr > msg)
        {
            msg_ptr--;
        }
        break;

    default:
        #if CFG_INTERFACE_SILENTMODE == 0
        printf("%c",c);
        #endif
        *msg_ptr++ = c;
        break;
  }
  return 0;
}

/**************************************************************************/
/*! 
    @brief  Displays the command prompt.  The text that appears is defined
            in projectconfig.h.
*/
/**************************************************************************/
static void cmdMenu()
{
  #if CFG_INTERFACE_SILENTMODE == 0
  printf(CFG_PRINTF_NEWLINE);
  printf(CFG_INTERFACE_PROMPT);
  #endif
  #if CFG_INTERFACE_CONFIRMREADY == 1
  printf("%s%s", CFG_INTERFACE_CONFIRMREADY_TEXT, CFG_PRINTF_NEWLINE);
  #endif
}

/**************************************************************************/
/*! 
    @brief  Parse the command line. This function tokenizes the command
            input, then searches for the command table entry associated
            with the commmand. Once found, it will jump to the
            corresponding function.

    @param[in]  cmd
                The entire command string to be parsed
*/
/**************************************************************************/
void cmdParse(char *cmd)
{
  size_t argc, i = 0;
  char *argv[30];

  argv[i] = strtok(cmd, " ");
  do
  {
      argv[++i] = strtok(NULL, " ");
  } while ((i < 30) && (argv[i] != NULL));
  
  argc = i;
  for (i=0; i < CMD_COUNT; i++)
  {
      if (!strcmp(argv[0], cmd_tbl[i].command))
      {
        if ((argc == 2) && !strcmp (argv [1], "?"))
        {
          // Display parameter help menu on 'command ?'
          printf ("%s%s%s", cmd_tbl[i].description, CFG_PRINTF_NEWLINE, CFG_PRINTF_NEWLINE);
          printf ("%s%s", cmd_tbl[i].parameters, CFG_PRINTF_NEWLINE);
        }
        else if ((argc - 1) < cmd_tbl[i].minArgs)
        {
          // Too few arguments supplied
          #if CFG_INTERFACE_SHORTERRORS == 1
          printf ("%s%s", CFG_INTERFACE_SHORTERRORS_TOOFEWARGS, CFG_PRINTF_NEWLINE);
          #else
          printf ("Too few arguments (%d expected)%s", cmd_tbl[i].minArgs, CFG_PRINTF_NEWLINE);
          printf ("%sType '%s ?' for more information%s%s", CFG_PRINTF_NEWLINE, cmd_tbl[i].command, CFG_PRINTF_NEWLINE, CFG_PRINTF_NEWLINE);
          #endif
        }
        else if ((argc - 1) > cmd_tbl[i].maxArgs)
        {
          // Too many arguments supplied
          #if CFG_INTERFACE_SHORTERRORS == 1
          printf ("%s%s", CFG_INTERFACE_SHORTERRORS_TOOMANYARGS, CFG_PRINTF_NEWLINE);
          #else
          printf ("Too many arguments (%d maximum)%s", cmd_tbl[i].maxArgs, CFG_PRINTF_NEWLINE);
          printf ("%sType '%s ?' for more information%s%s", CFG_PRINTF_NEWLINE, cmd_tbl[i].command, CFG_PRINTF_NEWLINE, CFG_PRINTF_NEWLINE);
          #endif
        }
        else
        {
          #if CFG_INTERFACE_ENABLEIRQ != 0
          // Set the IRQ pin high at start of a command
          gpioSetValue(CFG_INTERFACE_IRQPORT, CFG_INTERFACE_IRQPIN, 1);
          #endif
          // Dispatch command to the appropriate function
          cmd_tbl[i].func(argc - 1, &argv [1]);
          #if CFG_INTERFACE_ENABLEIRQ  != 0
          // Set the IRQ pin low to signal the end of a command
          gpioSetValue(CFG_INTERFACE_IRQPORT, CFG_INTERFACE_IRQPIN, 0);
          #endif
        }

        // Refresh the command prompt
        cmdMenu();
        return;
      }
  }
  // Command not recognized
  #if CFG_INTERFACE_SHORTERRORS == 1
  printf ("%s%s", CFG_INTERFACE_SHORTERRORS_UNKNOWNCOMMAND, CFG_PRINTF_NEWLINE);
  #else
  printf("Command not recognized: '%s'%s%s", cmd, CFG_PRINTF_NEWLINE, CFG_PRINTF_NEWLINE);
  #if CFG_INTERFACE_SILENTMODE == 0
  printf("Type '?' for a list of all available commands%s", CFG_PRINTF_NEWLINE);
  #endif
  #endif

  cmdMenu();
}

/**************************************************************************/
/*! 
    @brief Initialises the command line using the appropriate interface
*/
/**************************************************************************/
void cmdInit()
{
  #if defined CFG_INTERFACE && defined CFG_INTERFACE_UART
    // Check if UART is already initialised
    uart_pcb_t *pcb = uartGetPCB();
    if (!pcb->initialised)
    {
      uartInit(CFG_UART_BAUDRATE);
    }
  #endif

  #if CFG_INTERFACE_ENABLEIRQ != 0
    // Set IRQ pin as output
    gpioSetDir(CFG_INTERFACE_IRQPORT, CFG_INTERFACE_IRQPIN, gpioDirection_Output);
    gpioSetValue(CFG_INTERFACE_IRQPORT, CFG_INTERFACE_IRQPIN, 1);
  #endif

  // init the msg ptr
  msg_ptr = msg;

  #ifndef CFG_INTERFACE_SILENTINIT
  // Show the menu
  cmdMenu();
  #endif

  // Set the IRQ pin low by default
  #if CFG_INTERFACE_ENABLEIRQ  != 0
    gpioSetValue(CFG_INTERFACE_IRQPORT, CFG_INTERFACE_IRQPIN, 0);
  #endif
}

/**************************************************************************/
/*! 
    'help' command handler
*/
/**************************************************************************/
void cmd_help(uint8_t argc, char **argv)
{
  size_t i;

  printf("Command      Description%s", CFG_PRINTF_NEWLINE);
  printf("-------      -----------%s", CFG_PRINTF_NEWLINE);

  // Display full command list
  for (i=0; i < CMD_COUNT; i++)
  {
    if (!cmd_tbl[i].hidden) 
    {
      printf ("%-10s   %s%s", cmd_tbl[i].command, cmd_tbl[i].description, CFG_PRINTF_NEWLINE);
    }
  }

  printf("%sCommand parameters can be seen by entering: <command-name> ?%s", CFG_PRINTF_NEWLINE, CFG_PRINTF_NEWLINE);
}
