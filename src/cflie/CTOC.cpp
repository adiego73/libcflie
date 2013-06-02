#include "cflie/CTOC.h"


CTOC::CTOC(CCrazyRadio *crRadio, int nPort) {
  m_crRadio = crRadio;
  m_nPort = nPort;
  m_nItemCount = 0;
}

CTOC::~CTOC() {
}

bool CTOC::sendTOCPointerReset() {
  CCRTPPacket *crtpPacket = new CCRTPPacket(0x00, 0);
  crtpPacket->setPort(m_nPort);
  CCRTPPacket *crtpReceived = m_crRadio->sendPacket(crtpPacket, true);
  
  if(crtpReceived) {
    delete crtpReceived;
    return true;
  }
  
  return false;
}

bool CTOC::requestMetaData() {
  bool bReturnvalue = false;
  
  CCRTPPacket *crtpPacket = new CCRTPPacket(0x01, 0);
  crtpPacket->setPort(m_nPort);
  CCRTPPacket *crtpReceived = m_crRadio->sendAndReceive(crtpPacket);
  
  if(crtpReceived->data()[1] == 0x01) {
    m_nItemCount = crtpReceived->data()[2];
    bReturnvalue = true;
  }
  
  delete crtpReceived;
  return bReturnvalue;
}

bool CTOC::requestInitialItem() {
  return this->requestItem(0, true);
}

bool CTOC::requestItem(int nID) {
  return this->requestItem(nID, false);
}

bool CTOC::requestItem(int nID, bool bInitial) {
  bool bReturnvalue = false;
  
  char cRequest[2];
  cRequest[0] = 0x0;
  cRequest[1] = nID;
  
  CCRTPPacket *crtpPacket = new CCRTPPacket(cRequest,
					    (bInitial ? 1 : 2),
					    0);
  crtpPacket->setPort(m_nPort);
  CCRTPPacket *crtpReceived = m_crRadio->sendAndReceive(crtpPacket);
  
  bReturnvalue = this->processItem(crtpReceived);
  
  delete crtpReceived;
  return bReturnvalue;
}

bool CTOC::requestItems() {
  for(int nI = 0; nI < m_nItemCount; nI++) {
    this->requestItem(nI);
  }
  //this->requestInitialItem();
  
  return true;
}

bool CTOC::processItem(CCRTPPacket *crtpItem) {
  if(crtpItem->port() == m_nPort) {
    if(crtpItem->channel() == 0) {
      char *cData = crtpItem->data();
      int nLength = crtpItem->dataLength();
      
      if(cData[1] == 0x0) { // Command identification ok?
	int nID = cData[2];
	int nType = cData[3];
	
	string strGroup;
	int nI;
	for(nI = 4; cData[nI] != '\0'; nI++) {
	  strGroup += cData[nI];
	}
	
	nI++;
	string strIdentifier;
	for(; cData[nI] != '\0'; nI++) {
	  strIdentifier += cData[nI];
	}
	
	struct TOCElement teNew;
	teNew.strIdentifier = strIdentifier;
	teNew.strGroup = strGroup;
	teNew.nID = nID;
	teNew.nType = nType;
	teNew.bIsLogging = false;
	teNew.strValue = "";
	teNew.nValue = 0;
	teNew.dValue = 0;
	
	m_lstTOCElements.push_back(teNew);
	
	cout << strGroup << "." << strIdentifier << endl;
	
	return true;
      }
    }
  }
  
  return false;
}

struct TOCElement CTOC::elementForName(string strName, bool &bFound) {
  for(list<struct TOCElement>::iterator itElement = m_lstTOCElements.begin();
      itElement != m_lstTOCElements.end();
      itElement++) {
    struct TOCElement teCurrent = *itElement;
    
    string strTempFullname = teCurrent.strGroup + "." + teCurrent.strIdentifier;
    if(strName == strTempFullname) {
      bFound = true;
      return teCurrent;
    }
  }
  
  bFound = false;
  struct TOCElement teEmpty;
  
  return teEmpty;
}

int CTOC::idForName(string strName) {
  bool bFound;
  
  struct TOCElement teResult = this->elementForName(strName, bFound);
  
  if(bFound) {
    return teResult.nID;
  }
  
  return -1;
}

int CTOC::typeForName(string strName) {
  bool bFound;
  
  struct TOCElement teResult = this->elementForName(strName, bFound);
  
  if(bFound) {
    return teResult.nType;
  }
  
  return -1;
}

bool CTOC::startLogging(string strName, string strBlockName) {
  bool bFound;
  struct LoggingBlock lbCurrent = this->loggingBlockForName(strBlockName, bFound);
  
  if(bFound) {
    struct TOCElement teCurrent = this->elementForName(strName, bFound);
    if(bFound) {
      char cPayload[5] = {0x01, lbCurrent.nID, teCurrent.nType, teCurrent.nID};
      CCRTPPacket *crtpLogVariable = new CCRTPPacket(cPayload, 4, 1);
      crtpLogVariable->setPort(m_nPort);
      crtpLogVariable->setChannel(1);
      CCRTPPacket *crtpReceived = m_crRadio->sendAndReceive(crtpLogVariable, true);
      
      char *cData = crtpReceived->data();
      bool bCreateOK = false;
      if(cData[1] == 0x01 &&
	 cData[2] == lbCurrent.nID &&
	 cData[3] == 0x00) {
	bCreateOK = true;
      } else {
	cout << cData[3] << endl;
      }
      
      if(crtpReceived) {
	delete crtpReceived;
      }
      
      if(bCreateOK) {
	this->addElementToBlock(lbCurrent.nID, teCurrent.nID);
	
	return true;
      }
    }
  }
  
  return false;
}

bool CTOC::addElementToBlock(int nBlockID, int nElementID) {
  for(list<struct LoggingBlock>::iterator itBlock = m_lstLoggingBlocks.begin();
      itBlock != m_lstLoggingBlocks.end();
      itBlock++) {
    struct LoggingBlock lbCurrent = *itBlock;
    
    if(lbCurrent.nID == nBlockID) {
      (*itBlock).lstElementIDs.push_back(nElementID);
      
      return true;
    }
  }
  
  return false;
}

bool CTOC::stopLogging(string strName) {
  
}

bool CTOC::isLogging(string strName) {
  
}

string CTOC::stringValue(string strName) {
  bool bFound;
  
  struct TOCElement teResult = this->elementForName(strName, bFound);
  
  if(bFound) {
    return teResult.strValue;
  }
  
  return "";
}

double CTOC::doubleValue(string strName) {
  bool bFound;
  
  struct TOCElement teResult = this->elementForName(strName, bFound);
  
  if(bFound) {
    return teResult.dValue;
  }
  
  return 0;
}

int CTOC::integerValue(string strName) {
  bool bFound;
  
  struct TOCElement teResult = this->elementForName(strName, bFound);
  
  if(bFound) {
    return teResult.nValue;
  }
  
  return 0;
}

struct LoggingBlock CTOC::loggingBlockForName(string strName, bool &bFound) {
  for(list<struct LoggingBlock>::iterator itBlock = m_lstLoggingBlocks.begin();
      itBlock != m_lstLoggingBlocks.end();
      itBlock++) {
    struct LoggingBlock lbCurrent = *itBlock;
    
    if(strName == lbCurrent.strName) {
      bFound = true;
      return lbCurrent;
    }
  }
  
  bFound = false;
  struct LoggingBlock lbEmpty;
  
  return lbEmpty;
}

struct LoggingBlock CTOC::loggingBlockForID(int nID, bool &bFound) {
  for(list<struct LoggingBlock>::iterator itBlock = m_lstLoggingBlocks.begin();
      itBlock != m_lstLoggingBlocks.end();
      itBlock++) {
    struct LoggingBlock lbCurrent = *itBlock;
    
    if(nID == lbCurrent.nID) {
      bFound = true;
      return lbCurrent;
    }
  }
  
  bFound = false;
  struct LoggingBlock lbEmpty;
  
  return lbEmpty;
}

bool CTOC::registerLoggingBlock(string strName, double dFrequency) {
  int nID = 0;
  bool bFound;
  
  if(dFrequency > 0) { // Only do it if a valid frequency > 0 is given
    this->loggingBlockForName(strName, bFound);
    if(bFound) {
      this->unregisterLoggingBlock(strName);
    }
    
    do {
      this->loggingBlockForID(nID, bFound);
	
      if(bFound) {
	nID++;
      }
    } while(bFound);
    
    this->unregisterLoggingBlockID(nID);
    
    double d10thOfMS = (1 / dFrequency) * 1000 * 10;
    //    char cPayload[4] = {0x00, (nID >> 16) & 0x00ff, nID & 0x00ff, d10thOfMS};
    char cPayload[4] = {0x00, nID, d10thOfMS};
    CCRTPPacket *crtpRegisterBlock = new CCRTPPacket(cPayload, 3, 1);
    crtpRegisterBlock->setPort(m_nPort);
    crtpRegisterBlock->setChannel(1);
    
    CCRTPPacket *crtpReceived = m_crRadio->sendAndReceive(crtpRegisterBlock, true);
    
    char *cData = crtpReceived->data();
    bool bCreateOK = false;
    if(cData[1] == 0x00 &&
       cData[2] == nID &&
       cData[3] == 0x00) {
      bCreateOK = true;
      cout << "Registered logging block `" << strName << "'" << endl;
    }
    
    if(crtpReceived) {
      delete crtpReceived;
    }
      
    if(bCreateOK) {
      struct LoggingBlock lbNew;
      lbNew.strName = strName;
      lbNew.nID = nID;
      lbNew.dFrequency = dFrequency;
	
      m_lstLoggingBlocks.push_back(lbNew);
	
      return this->enableLogging(strName);
    }
  }
  
  return false;
}

bool CTOC::enableLogging(string strBlockName) {
  bool bFound;
  
  struct LoggingBlock lbCurrent = this->loggingBlockForName(strBlockName, bFound);
  if(bFound) {
    double d10thOfMS = (1 / lbCurrent.dFrequency) * 1000 * 10;
    char cPayload[3] = {0x03, lbCurrent.nID, d10thOfMS};
    
    CCRTPPacket *crtpEnable = new CCRTPPacket(cPayload, 3, 1);
    crtpEnable->setPort(m_nPort);
    crtpEnable->setChannel(1);
    
    CCRTPPacket *crtpReceived = m_crRadio->sendAndReceive(crtpEnable);
    delete crtpReceived;
    
    return true;
  }
  
  return false;
}

bool CTOC::unregisterLoggingBlock(string strName) {
  bool bFound;
  
  struct LoggingBlock lbCurrent = this->loggingBlockForName(strName, bFound);
  if(bFound) {
    return this->unregisterLoggingBlockID(lbCurrent.nID);
  }
  
  return false;
}

bool CTOC::unregisterLoggingBlockID(int nID) {
  char cPayload[2] = {0x02, nID};
  CCRTPPacket *crtpUnregisterBlock = new CCRTPPacket(cPayload, 2, 1);
  crtpUnregisterBlock->setPort(m_nPort);
  crtpUnregisterBlock->setChannel(1);
  CCRTPPacket *crtpReceived = m_crRadio->sendAndReceive(crtpUnregisterBlock, true);
  
  if(crtpReceived) {
    delete crtpReceived;
    return true;
  }
  
  return false;
}

void CTOC::processPackets(list<CCRTPPacket*> lstPackets) {
  if(lstPackets.size() > 0) {
    for(list<CCRTPPacket*>::iterator itPacket = lstPackets.begin();
	itPacket != lstPackets.end();
	itPacket++) {
      CCRTPPacket *crtpPacket = *itPacket;
      
      char *cData = crtpPacket->data();
      float fValue;
      memcpy(&fValue, &cData[5], 4);
      //cout << fValue << endl;
      
      char *cLogdata = &cData[5];
      int nOffset = 0;
      int nIndex = 0;
      int nAvailableLogBytes = crtpPacket->dataLength() - 5;
      
      int nBlockID = cData[1];
      bool bFound;
      struct LoggingBlock lbCurrent = this->loggingBlockForID(nBlockID, bFound);
      
      if(bFound) {
	while(nIndex < lbCurrent.lstElementIDs.size()) {
	  // Only float values supported at the moment
	  float fValue;
	  memcpy(&fValue, &cLogdata[nOffset], sizeof(float));
	  
	  int nElementID = this->elementIDinBlock(nBlockID, nIndex);
	  this->setFloatValueForElementID(nElementID, fValue);
	  
	  nOffset += sizeof(float);
	  nIndex++;
	}
      }
      
      delete crtpPacket;
    }
  }
}

int CTOC::elementIDinBlock(int nBlockID, int nElementIndex) {
  bool bFound;
  
  struct LoggingBlock lbCurrent = this->loggingBlockForID(nBlockID, bFound);
  if(bFound) {
    if(nElementIndex < lbCurrent.lstElementIDs.size()) {
      list<int>::iterator itID = lbCurrent.lstElementIDs.begin();
      advance(itID, nElementIndex);
      return *itID;
    }
  }
  
  return -1;
}

bool CTOC::setFloatValueForElementID(int nElementID, float fValue) {
  int nIndex = 0;
  for(list<struct TOCElement>::iterator itElement = m_lstTOCElements.begin();
      itElement != m_lstTOCElements.end();
      itElement++, nIndex++) {
    struct TOCElement teCurrent = *itElement;
    
    if(teCurrent.nID == nElementID) {
      teCurrent.dValue = fValue; // We store floats as doubles
      (*itElement) = teCurrent;
      // cout << fValue << endl;
      return true;
    }
  }
  
  return false;
}