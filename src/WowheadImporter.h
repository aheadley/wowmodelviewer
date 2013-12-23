/*
 * WowheadImporter.h
 *
 *  Created on: 1 déc. 2013
 *
 */

#ifndef _WOWHEADIMPORTER_H_
#define _WOWHEADIMPORTER_H_

#include "URLImporter.h"

class WowheadImporter : public URLImporter
{
  public:
	WowheadImporter();
	~WowheadImporter();

	NPCInfos * importNPC(std::string url);
	ItemRecord * importItem(std::string url);

  private:
	std::string extractSubString(std::string & datas, std::string beginPattern, std::string endPattern);
};


#endif /* _WOWHEADIMPORTER_H_ */
