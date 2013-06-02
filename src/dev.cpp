// Copyright (c) 2013, Jan Winkler <winkler@cs.uni-bremen.de>
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of Universität Bremen nor the names of its
//       contributors may be used to endorse or promote products derived from
//       this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include <iostream>
#include <signal.h>
#include <termios.h>
#include <fcntl.h>

#include <cflie/CCrazyflie.h>

using namespace std;


bool g_bGoon;


void interruptionHandler(int dummy = 0) {
  g_bGoon = false;
}


int main(int argc, char **argv) {
  signal(SIGINT, interruptionHandler);
  
  int kfd = 0;
  char c;
  struct termios cooked, raw;
  tcgetattr(kfd, &cooked);
  memcpy(&raw, &cooked, sizeof(struct termios));
  raw.c_lflag &=~ (ICANON | ECHO);
  raw.c_cc[VEOL] = 1;
  raw.c_cc[VEOF] = 2;
  tcsetattr(kfd, TCSANOW, &raw);
  
  fcntl(kfd, F_SETFL, O_NONBLOCK);
  
  int nReturnvalue = 0;
  int nThrust = 0;
  
  string strRadioURI = "radio://0/10/250K";
  
  cout << "Opening radio URI '" << strRadioURI << "'" << endl;
  CCrazyRadio *crRadio = new CCrazyRadio(strRadioURI);
  
  g_bGoon = true;
  bool bDongleConnected = false;
  bool bDongleNotConnectedNotified = false;
  
  while(g_bGoon) {
    // Is the dongle connected? If not, try to connect it.
    if(!bDongleConnected) {
      while(!crRadio->startRadio() && g_bGoon) {
	if(!bDongleNotConnectedNotified) {
	  cout << "Waiting for dongle." << endl;
	  bDongleNotConnectedNotified = true;
	}
	
	sleep(0.5);
      }
      
      if(g_bGoon) {
	cout << "Dongle connected, radio started." << endl;
      }
    }
    
    bool bRangeStateChangedNotified = false;
    bool bCopterWasInRange = false;
    
    if(g_bGoon) {
      bDongleNotConnectedNotified = false;
      bDongleConnected = true;
      
      CCrazyflie *cflieCopter = new CCrazyflie(crRadio);
      cflieCopter->setThrust(nThrust);
      
      while(g_bGoon && bDongleConnected) {
	if(cflieCopter->cycle()) {
	  usleep(1000);
	  if(cflieCopter->isInitialized()) {
	    if(cflieCopter->copterInRange()) {
	      if(!bCopterWasInRange || !bRangeStateChangedNotified) {
		// Event triggered when the copter first comes in range.
		cout << "In range" << endl;
		
		// Register the desired sensor readings
		cflieCopter->enableStabilizerLogging();
		cflieCopter->enableGyroscopeLogging();
		cflieCopter->enableAccelerometerLogging();
		/*cflieCopter->addHighSpeedLogging("stabilizer.roll");
		cflieCopter->addHighSpeedLogging("stabilizer.pitch");
		cflieCopter->addHighSpeedLogging("stabilizer.yaw");*/
		/*cflieCopter->addHighSpeedLogging("gyro.x");
		cflieCopter->addHighSpeedLogging("gyro.y");
		cflieCopter->addHighSpeedLogging("gyro.z");*/
		/*cflieCopter->addHighSpeedLogging("acc.x");
		  cflieCopter->addHighSpeedLogging("acc.y");
		  cflieCopter->addHighSpeedLogging("acc.x");*/
		//cflieCopter->enableHighSpeedLogging();
		
		bCopterWasInRange = true;
		bRangeStateChangedNotified = true;
		
		cflieCopter->setSendSetpoints(true);
	      }
	      
	      // Loop body for when the copter is in range continuously
	      int nThrust = 0;//10001;
	      
	      int nReturnRead = read(kfd, &c, 1);
	      if(nReturnRead >= 0) {
		if(c == 32) {
		  nThrust = 35000;
		}
	      }
	      
	      cflieCopter->setThrust(nThrust);
	      cout << "Roll: " << cflieCopter->sensorDoubleValue("stabilizer.roll") << endl;
	      cout << "Pitch: " << cflieCopter->sensorDoubleValue("stabilizer.pitch") << endl;
	      cout << "Yaw: " << cflieCopter->sensorDoubleValue("stabilizer.yaw") << endl;
	      cout << "Acc x: " << cflieCopter->sensorDoubleValue("acc.x") << endl;
	      cout << "Gyro x: " << cflieCopter->sensorDoubleValue("gyro.x") << endl;

	    } else {
	      if(bCopterWasInRange || !bRangeStateChangedNotified) {
		// Event triggered when the copter leaves the range.
		cout << "Not in range" << endl;
	      
		bCopterWasInRange = false;
		bRangeStateChangedNotified = true;
	      }
	    
	      // Loop body for when the copter is not in range
	    }
	  }
	} else {
	  cerr << "Connection to radio dongle lost." << endl;
	  
	  bDongleConnected = false;
	}
      }
      
      if(!g_bGoon) {
	// NOTE(winkler): Here would be the perfect place to initiate a
	// landing sequence (i.e. ramping down the altitude of the
	// copter). Right now, this is a dummy branch.
	cflieCopter->disableLogging();
	cflieCopter->disableStabilizerLogging();
	cflieCopter->disableGyroscopeLogging();
	cflieCopter->disableAccelerometerLogging();
      }
      
      delete cflieCopter;
    }
  }
  
  cout << "Cleaning up" << endl;
  tcsetattr(kfd, TCSANOW, &cooked);
  delete crRadio;
  
  return nReturnvalue;
}