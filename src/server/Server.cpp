/*
	This file is part of Spine.

    Spine is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Spine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
 */
// Copyright 2018 Clockwork Origins

#include "Server.h"

#include <fstream>
#include <map>
#include <set>
#include <thread>

#include "DatabaseCreator.h"
#include "DownloadSizeChecker.h"
#include "GMPServer.h"
#include "MariaDBWrapper.h"
#include "MatchmakingServer.h"
#include "SpineServerConfig.h"
#include "Smtp.h"
#include "UploadServer.h"

#include "common/MessageStructs.h"

#include "boost/filesystem.hpp"

#include "clockUtils/sockets/TcpSocket.h"

#include "tinyxml2.h"

namespace spine {

	static const std::string DEFAULTURL = "https://clockwork-origins.de/Gothic/downloads/mods/";

	Server::Server() : _listenClient(new clockUtils::sockets::TcpSocket()), _listenMPServer(new clockUtils::sockets::TcpSocket()), _downloadSizeChecker(new DownloadSizeChecker()), _matchmakingServer(new MatchmakingServer()), _newsLock(), _gmpServer(new GMPServer()), _uploadServer(new UploadServer()) {
		DatabaseCreator::createTables();
	}

	Server::~Server() {
		delete _listenClient;
		delete _listenMPServer;
		delete _downloadSizeChecker;
		delete _matchmakingServer;
		delete _gmpServer;
		delete _uploadServer;
	}

	int Server::run() {
		if (_listenClient->listen(SERVER_PORT, 10, true, std::bind(&Server::accept, this, std::placeholders::_1)) != clockUtils::ClockError::SUCCESS) {
			return 1;
		}
		_uploadServer->run();
#ifndef TEST_CONFIG
		_gmpServer->run();
#endif

		bool running = true;

		while (running) {
			std::cout << "Possible actions:" << std::endl;
			std::cout << "\tx:\t\tshutdown server" << std::endl;
			std::cout << "\tc:\t\tclear download sizes" << std::endl;

			const int c = getchar();

			switch (c) {
			case 'x': {
				running = false;
				break;
			}
			case 'c': {
				_downloadSizeChecker->clear();
				break;
			}
			default: {
				break;
			}
			}
		}

		return 0;
	}

	void Server::accept(clockUtils::sockets::TcpSocket * sock) {
		sock->receiveCallback(std::bind(&Server::receiveMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	}

	void Server::receiveMessage(const std::vector<uint8_t> & message, clockUtils::sockets::TcpSocket * sock, clockUtils::ClockError error) const {
		if (error != clockUtils::ClockError::SUCCESS) {
			std::thread([sock]() {
				sock->close();
				delete sock;
			}).detach();
		} else {
			try {
				common::Message * m = common::Message::DeserializeBlank(std::string(message.begin(), message.end()));
				if (!m) {
					m = common::Message::DeserializePrivate(std::string(message.begin(), message.end()));
					if (!m) {
						sock->writePacket("Crash"); // it's a hack to stop hanging threads
						return;
					}
				}
				if (m->type == common::MessageType::UPDATEREQUEST) {
					common::UpdateRequestMessage * msg = dynamic_cast<common::UpdateRequestMessage *>(m);
					handleAutoUpdate(sock, msg);
				} else if (m->type == common::MessageType::REQUESTALLMODS) {
					common::RequestAllModsMessage * msg = dynamic_cast<common::RequestAllModsMessage *>(m);
					handleModListRequest(sock, msg);
				} else if (m->type == common::MessageType::REQUESTMODFILES) {
					common::RequestModFilesMessage * msg = dynamic_cast<common::RequestModFilesMessage *>(m);
					handleModFilesListRequest(sock, msg);
				} else if (m->type == common::MessageType::DOWNLOADSUCCEEDED) {
					common::DownloadSucceededMessage * msg = dynamic_cast<common::DownloadSucceededMessage *>(m);
					handleDownloadSucceeded(sock, msg);
				} else if (m->type == common::MessageType::UPDATEPLAYTIME) {
					common::UpdatePlayTimeMessage * msg = dynamic_cast<common::UpdatePlayTimeMessage *>(m);
					handleUpdatePlaytime(sock, msg);
				} else if (m->type == common::MessageType::REQUESTPLAYTIME) {
					common::RequestPlayTimeMessage * msg = dynamic_cast<common::RequestPlayTimeMessage *>(m);
					handleRequestPlaytime(sock, msg);
				} else if (m->type == common::MessageType::REQUESTSCORES) {
					common::RequestScoresMessage * msg = dynamic_cast<common::RequestScoresMessage *>(m);
					handleRequestScores(sock, msg);
				} else if (m->type == common::MessageType::UPDATESCORE) {
					common::UpdateScoreMessage * msg = dynamic_cast<common::UpdateScoreMessage *>(m);
					handleUpdateScore(sock, msg);
				} else if (m->type == common::MessageType::REQUESTACHIEVEMENTS) {
					common::RequestAchievementsMessage * msg = dynamic_cast<common::RequestAchievementsMessage *>(m);
					handleRequestAchievements(sock, msg);
				} else if (m->type == common::MessageType::UNLOCKACHIEVEMENT) {
					common::UnlockAchievementMessage * msg = dynamic_cast<common::UnlockAchievementMessage *>(m);
					handleUnlockAchievement(sock, msg);
				} else if (m->type == common::MessageType::MODVERSIONCHECK) {
					common::ModVersionCheckMessage * msg = dynamic_cast<common::ModVersionCheckMessage *>(m);
					handleModVersionCheck(sock, msg);
				} else if (m->type == common::MessageType::FEEDBACK) {
					common::FeedbackMessage * msg = dynamic_cast<common::FeedbackMessage *>(m);
					handleFeedback(sock, msg);
				} else if (m->type == common::MessageType::REQUESTORIGINALFILES) {
					common::RequestOriginalFilesMessage * msg = dynamic_cast<common::RequestOriginalFilesMessage *>(m);
					handleRequestOriginalFiles(sock, msg);
				} else if (m->type == common::MessageType::UPDATELOGINTIME) {
					common::UpdateLoginTimeMessage * msg = dynamic_cast<common::UpdateLoginTimeMessage *>(m);
					handleUpdateLoginTime(sock, msg);
				} else if (m->type == common::MessageType::REQUESTPACKAGEFILES) {
					common::RequestPackageFilesMessage * msg = dynamic_cast<common::RequestPackageFilesMessage *>(m);
					handleRequestPackageFiles(sock, msg);
				} else if (m->type == common::MessageType::PACKAGEDOWNLOADSUCCEEDED) {
					common::PackageDownloadSucceededMessage * msg = dynamic_cast<common::PackageDownloadSucceededMessage *>(m);
					handlePackageDownloadSucceeded(sock, msg);
				} else if (m->type == common::MessageType::REQUESTALLMODSTATS) {
					common::RequestAllModStatsMessage * msg = dynamic_cast<common::RequestAllModStatsMessage *>(m);
					handleRequestAllModStats(sock, msg);
				} else if (m->type == common::MessageType::REQUESTSINGLEMODSTAT) {
					common::RequestSingleModStatMessage * msg = dynamic_cast<common::RequestSingleModStatMessage *>(m);
					handleRequestSingleModStat(sock, msg);
				} else if (m->type == common::MessageType::REQUESTALLACHIEVEMENTSTATS) {
					common::RequestAllAchievementStatsMessage * msg = dynamic_cast<common::RequestAllAchievementStatsMessage *>(m);
					handleRequestAllAchievementStats(sock, msg);
				} else if (m->type == common::MessageType::REQUESTALLSCORESTATS) {
					common::RequestAllScoreStatsMessage * msg = dynamic_cast<common::RequestAllScoreStatsMessage *>(m);
					handleRequestAllScoreStats(sock, msg);
				} else if (m->type == common::MessageType::REQUESTALLNEWS) {
					common::RequestAllNewsMessage * msg = dynamic_cast<common::RequestAllNewsMessage *>(m);
					handleRequestAllNews(sock, msg);
				} else if (m->type == common::MessageType::SUBMITNEWS) {
					common::SubmitNewsMessage * msg = dynamic_cast<common::SubmitNewsMessage *>(m);
					handleSubmitNews(sock, msg);
				} else if (m->type == common::MessageType::LINKCLICKED) {
					common::LinkClickedMessage * msg = dynamic_cast<common::LinkClickedMessage *>(m);
					handleLinkClicked(sock, msg);
				} else if (m->type == common::MessageType::SUBMITSCRIPTFEATURES) {
					common::SubmitScriptFeaturesMessage * msg = dynamic_cast<common::SubmitScriptFeaturesMessage *>(m);
					handleSubmitScriptFeatures(sock, msg);
				} else if (m->type == common::MessageType::REQUESTINFOPAGE) {
					common::RequestInfoPageMessage * msg = dynamic_cast<common::RequestInfoPageMessage *>(m);
					handleRequestInfoPage(sock, msg);
				} else if (m->type == common::MessageType::SUBMITINFOPAGE) {
					common::SubmitInfoPageMessage * msg = dynamic_cast<common::SubmitInfoPageMessage *>(m);
					handleSubmitInfoPage(sock, msg);
				} else if (m->type == common::MessageType::SENDUSERINFOS) {
					common::SendUserInfosMessage * msg = dynamic_cast<common::SendUserInfosMessage *>(m);
					handleSendUserInfos(sock, msg);
				} else if (m->type == common::MessageType::REQUESTRANDOMMOD) {
					common::RequestRandomModMessage * msg = dynamic_cast<common::RequestRandomModMessage *>(m);
					handleRequestRandomMod(sock, msg);
				} else if (m->type == common::MessageType::UPDATEACHIEVEMENTPROGRESS) {
					common::UpdateAchievementProgressMessage * msg = dynamic_cast<common::UpdateAchievementProgressMessage *>(m);
					handleUpdateAchievementProgress(sock, msg);
				} else if (m->type == common::MessageType::SUBMITCOMPATIBILITY) {
					common::SubmitCompatibilityMessage * msg = dynamic_cast<common::SubmitCompatibilityMessage *>(m);
					handleSubmitCompatibility(sock, msg);
				} else if (m->type == common::MessageType::REQUESTOWNCOMPATIBILITIES) {
					common::RequestOwnCompatibilitiesMessage * msg = dynamic_cast<common::RequestOwnCompatibilitiesMessage *>(m);
					handleRequestOwnCompatibilities(sock, msg);
				} else if (m->type == common::MessageType::REQUESTCOMPATIBILITYLIST) {
					common::RequestCompatibilityListMessage * msg = dynamic_cast<common::RequestCompatibilityListMessage *>(m);
					handleRequestCompatibilityList(sock, msg);
				} else if (m->type == common::MessageType::REQUESTRATING) {
					common::RequestRatingMessage * msg = dynamic_cast<common::RequestRatingMessage *>(m);
					handleRequestRating(sock, msg);
				} else if (m->type == common::MessageType::SUBMITRATING) {
					common::SubmitRatingMessage * msg = dynamic_cast<common::SubmitRatingMessage *>(m);
					handleSubmitRating(sock, msg);
				} else if (m->type == common::MessageType::UPDATEREQUESTENCRYPTED) {
					common::UpdateRequestEncryptedMessage * msg = dynamic_cast<common::UpdateRequestEncryptedMessage *>(m);
					handleAutoUpdateEncrypted(sock, msg);
				} else if (m->type == common::MessageType::REQUESTOVERALLSAVEDATA) {
					common::RequestOverallSaveDataMessage * msg = dynamic_cast<common::RequestOverallSaveDataMessage *>(m);
					handleRequestOverallSaveData(sock, msg);
				} else if (m->type == common::MessageType::UPDATEOVERALLSAVEDATA) {
					common::UpdateOverallSaveDataMessage * msg = dynamic_cast<common::UpdateOverallSaveDataMessage *>(m);
					handleUpdateOverallSaveData(sock, msg);
				} else if (m->type == common::MessageType::REQUESTMODMANAGEMENT) {
					common::RequestModManagementMessage * msg = dynamic_cast<common::RequestModManagementMessage *>(m);
					handleRequestModManagement(sock, msg);
				} else if (m->type == common::MessageType::UPDATEMODVERSION) {
					common::UpdateModVersionMessage * msg = dynamic_cast<common::UpdateModVersionMessage *>(m);
					handleUpdateModVersion(sock, msg);
				} else if (m->type == common::MessageType::UPDATEEARLYACCESSSTATE) {
					common::UpdateEarlyAccessStateMessage * msg = dynamic_cast<common::UpdateEarlyAccessStateMessage *>(m);
					handleUpdateEarlyAccessState(sock, msg);
				} else if (m->type == common::MessageType::REQUESTMODSFOREDITOR) {
					common::RequestModsForEditorMessage * msg = dynamic_cast<common::RequestModsForEditorMessage *>(m);
					handleRequestModsForEditor(sock, msg);
				} else if (m->type == common::MessageType::UPDATEOFFLINEDATA) {
					common::UpdateOfflineDataMessage * msg = dynamic_cast<common::UpdateOfflineDataMessage *>(m);
					handleUpdateOfflineData(sock, msg);
				} else if (m->type == common::MessageType::REQUESTOFFLINEDATA) {
					common::RequestOfflineDataMessage * msg = dynamic_cast<common::RequestOfflineDataMessage *>(m);
					handleRequestOfflineData(sock, msg);
				} else if (m->type == common::MessageType::UPDATESTARTTIME) {
					common::UpdateStartTimeMessage * msg = dynamic_cast<common::UpdateStartTimeMessage *>(m);
					handleUpdateStartTime(sock, msg);
				} else if (m->type == common::MessageType::UPDATEPLAYINGTIME) {
					common::UpdatePlayingTimeMessage * msg = dynamic_cast<common::UpdatePlayingTimeMessage *>(m);
					handleUpdatePlayingTime(sock, msg);
				} else if (m->type == common::MessageType::REQUESTALLFRIENDS) {
					common::RequestAllFriendsMessage * msg = dynamic_cast<common::RequestAllFriendsMessage *>(m);
					handleRequestAllFriends(sock, msg);
				} else if (m->type == common::MessageType::SENDFRIENDREQUEST) {
					common::SendFriendRequestMessage * msg = dynamic_cast<common::SendFriendRequestMessage *>(m);
					handleSendFriendRequest(sock, msg);
				} else if (m->type == common::MessageType::ACCEPTFRIENDREQUEST) {
					common::AcceptFriendRequestMessage * msg = dynamic_cast<common::AcceptFriendRequestMessage *>(m);
					handleAcceptFriendRequest(sock, msg);
				} else if (m->type == common::MessageType::DECLINEFRIENDREQUEST) {
					common::DeclineFriendRequestMessage * msg = dynamic_cast<common::DeclineFriendRequestMessage *>(m);
					handleDeclineFriendRequest(sock, msg);
				} else if (m->type == common::MessageType::REQUESTUSERLEVEL) {
					common::RequestUserLevelMessage * msg = dynamic_cast<common::RequestUserLevelMessage *>(m);
					handleRequestUserLevel(sock, msg);
				} else if (m->type == common::MessageType::UPDATEGENERALMODCONFIGURATION) {
					common::UpdateGeneralModConfigurationMessage * msg = dynamic_cast<common::UpdateGeneralModConfigurationMessage *>(m);
					handleUpdateGeneralModConfiguration(sock, msg);
				} else if (m->type == common::MessageType::UPDATESCORES) {
					common::UpdateScoresMessage * msg = dynamic_cast<common::UpdateScoresMessage *>(m);
					handleUpdateScores(sock, msg);
				} else if (m->type == common::MessageType::UPDATEACHIEVEMENTS) {
					common::UpdateAchievementsMessage * msg = dynamic_cast<common::UpdateAchievementsMessage *>(m);
					handleUpdateAchievements(sock, msg);
				} else if (m->type == common::MessageType::UPDATESUCCEEDED) {
					common::UpdateSucceededMessage * msg = dynamic_cast<common::UpdateSucceededMessage *>(m);
					handleUpdateSucceeded(sock, msg);
				} else if (m->type == common::MessageType::UPDATECHAPTERSTATS) {
					common::UpdateChapterStatsMessage * msg = dynamic_cast<common::UpdateChapterStatsMessage *>(m);
					handleUpdateChapterStats(sock, msg);
				} else if (m->type == common::MessageType::UPDATEIMPRESSION) {
					common::UpdateImpressionMessage * msg = dynamic_cast<common::UpdateImpressionMessage *>(m);
					handleUpdateImpression(sock, msg);
				} else {
					std::cerr << "unexpected control message arrived: " << int(m->type) << std::endl;
					delete m;
					return;
				}

				delete m;
			} catch (const boost::archive::archive_exception &) {
				std::cerr << "deserialization not working" << std::endl;
				sock->writePacket("Crash"); // it's a hack to stop hanging threads
				return;
			} catch (std::system_error & err) {
				std::cout << "System Error: " << err.what() << std::endl;
			} catch (std::exception & err) {
				std::cout << "Exception: " << err.what() << std::endl;
			} catch (...) {
				std::cout << "Unhandled exception" << std::endl;
			}
		}
	}

	void Server::handleAutoUpdate(clockUtils::sockets::TcpSocket * sock, common::UpdateRequestMessage * msg) const {
		// 1. read xml
		tinyxml2::XMLDocument doc;

		const tinyxml2::XMLError e = doc.LoadFile("Spine_Version.xml");

		if (e) {
			std::cerr << "Couldn't open xml file!" << std::endl;
			common::UpdateFileCountMessage ufcm;
			ufcm.count = 0;

			const std::string serialized = ufcm.SerializeBlank();

			sock->writePacket(serialized);
			return;
		}

		uint32_t lastVersion = (msg->majorVersion << 16) + (msg->minorVersion << 8) + msg->patchVersion;

		std::map<uint32_t, std::vector<std::pair<std::string, std::string>>> versions;
		versions.insert(std::make_pair(lastVersion, std::vector<std::pair<std::string, std::string>>()));

		for (tinyxml2::XMLElement * node = doc.FirstChildElement("Version"); node != nullptr; node = node->NextSiblingElement("Version")) {
			const uint8_t majorVersion = uint8_t(std::stoi(node->Attribute("majorVersion")));
			const uint8_t minorVersion = uint8_t(std::stoi(node->Attribute("minorVersion")));
			const uint8_t patchVersion = uint8_t(std::stoi(node->Attribute("patchVersion")));
			uint32_t version = (majorVersion << 16) + (minorVersion << 8) + patchVersion;

			versions.insert(std::make_pair(version, std::vector<std::pair<std::string, std::string>>()));

			auto it = versions.find(version);

			for (tinyxml2::XMLElement * file = node->FirstChildElement("File"); file != nullptr; file = file->NextSiblingElement("File")) {
				it->second.emplace_back(file->GetText(), file->Attribute("Hash"));
			}
		}

		// 2. find all changed files
		std::set<std::pair<std::string, std::string>> files;

		auto it = versions.find(lastVersion);
		if (it != versions.end()) {
			++it;
			for (; it != versions.end(); ++it) {
				for (const std::pair<std::string, std::string> & file : it->second) {
					files.insert(file);
				}
				lastVersion = it->first;
			}
		}

		// 3. send file count
		common::UpdateFilesMessage ufm;
		for (auto & p : files) {
			ufm.files.push_back(p);
		}
		const std::string serialized = ufm.SerializeBlank();
		sock->writePacket(serialized);
	}

	void Server::handleModListRequest(clockUtils::sockets::TcpSocket * sock, common::RequestAllModsMessage * msg) const {
		MariaDBWrapper database;
		if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
			std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		std::map<int32_t, std::string> teamMembers;
		{
			if (!database.query(std::string("SELECT TeamID FROM teams;"))) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			auto lastResults = database.getResults<std::vector<std::string>>();
			if (!database.query("PREPARE selectTeamnameStmt FROM \"SELECT CAST(Name AS BINARY) FROM teamnames WHERE TeamID = ? AND Language = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramLanguage='" + msg->language + "';")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			for (auto vec : lastResults) {
				const int32_t id = int32_t(std::stoi(vec[0]));
				if (!database.query("SET @paramTeamID=" + std::to_string(id) + ";")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					return;
				}
				if (!database.query("EXECUTE selectTeamnameStmt USING @paramTeamID, @paramLanguage;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					return;
				}
				auto results = database.getResults<std::vector<std::string>>();
				if (results.size() == 1 && results[0].size() == 1) {
					teamMembers.insert(std::make_pair(id, results[0][0]));
				}
			}
		}
		// ModID | Name of the Mod shown in GUI | ID of the team | Enabled in GUI or only internal | 1 or 2 either if the mod is for Gothic 1 or 2 | Release date encoded as integer in days | one of Mod or Patch encoded as integer | major version | minor version | patch version
		if (!database.query(std::string("SELECT ModID, TeamID, Gothic, ReleaseDate, Type, MajorVersion, MinorVersion, PatchVersion FROM mods WHERE Enabled = 1;"))) {
			std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		auto enabledResults = database.getResults<std::vector<std::string>>();
		const int userID = getUserID(msg->username, msg->password);
		if (userID != -1) {
			if (!database.query(std::string("SELECT ModID, TeamID, Gothic, ReleaseDate, Type, MajorVersion, MinorVersion, PatchVersion FROM mods WHERE Enabled = 0;"))) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			auto disabledResults = database.getResults<std::vector<std::string>>();
			if (!database.query("PREPARE selectIsTeammemberStmt FROM \"SELECT UserID FROM teammembers WHERE TeamID = ? AND UserID = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("PREPARE selectEarlyUnlockStmt FROM \"SELECT * FROM earlyUnlocks WHERE ModID = ? AND UserID = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			for (auto vec : disabledResults) {
				if (!database.query("SET @paramTeamID=" + vec[1] + ";")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					return;
				}
				if (!database.query("EXECUTE selectIsTeammemberStmt USING @paramTeamID, @paramUserID;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					return;
				}
				auto allowed = database.getResults<std::vector<std::string>>();
				if (!allowed.empty()) {
					enabledResults.push_back(vec);
					continue;
				}
				if (!database.query("SET @paramModID=" + vec[0] + ";")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					return;
				}
				if (!database.query("EXECUTE selectEarlyUnlockStmt USING @paramModID, @paramUserID;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					return;
				}
				allowed = database.getResults<std::vector<std::string>>();
				if (!allowed.empty()) {
					enabledResults.push_back(vec);
				}
			}
		}
		common::UpdateAllModsMessage uamm;
		if (!database.query("PREPARE selectNodnameStmt FROM \"SELECT CAST(Name AS BINARY) FROM modnames WHERE ModID = ? AND Language = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectDevtimeStmt FROM \"SELECT Duration FROM devtimes WHERE ModID = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectTimeStmt FROM \"SELECT IFNULL(SUM(Duration), 0), IFNULL(COUNT(Duration), 0) FROM playtimes WHERE ModID = ? AND UserID != -1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		std::map<int32_t, uint32_t> versions;
		for (auto vec : enabledResults) {
			common::Mod mod;
			mod.id = std::stoi(vec[0]);
			if (!database.query("SET @paramModID=" + vec[0] + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("EXECUTE selectNodnameStmt USING @paramModID, @paramLanguage;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			auto results = database.getResults<std::vector<std::string>>();
			if (results.empty() || results[0].empty()) {
				continue;
			}
			mod.name = results[0][0];
			mod.teamID = std::stoi(vec[1]);
			const auto it = teamMembers.find(mod.teamID);
			if (it != teamMembers.end()) {
				mod.teamName = it->second;
			}
			mod.gothic = common::GothicVersion(std::stoi(vec[2]));
			mod.releaseDate = std::stoi(vec[3]);
			mod.type = common::ModType(std::stoi(vec[4]));
			mod.majorVersion = int8_t(std::stoi(vec[5]));
			mod.minorVersion = int8_t(std::stoi(vec[6]));
			mod.patchVersion = int8_t(std::stoi(vec[7]));
			if (!database.query("EXECUTE selectDevtimeStmt USING @paramModID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				continue;
			}
			results = database.getResults<std::vector<std::string>>();
			if (!results.empty()) {
				mod.devDuration = std::stoi(results[0][0]);
			} else {
				mod.devDuration = -1;
			}
			if (!database.query("EXECUTE selectTimeStmt USING @paramModID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				continue;
			}
			results = database.getResults<std::vector<std::string>>();
			if (!results.empty()) {
				const int sumDurations = std::stoi(results[0][0]);
				const int countDurations = std::stoi(results[0][1]);
				if (countDurations >= 1) {
					mod.avgDuration = sumDurations / countDurations;
				} else {
					mod.avgDuration = -1;
				}
			} else {
				mod.avgDuration = -1;
			}
			uint32_t version = (mod.majorVersion << 16) + (mod.minorVersion << 8) + mod.patchVersion;
			mod.downloadSize = _downloadSizeChecker->getBytes(mod.id, msg->language, version);
			uamm.mods.push_back(mod);
			versions.insert(std::make_pair(mod.id, version));
		}
		std::string serialized = uamm.SerializePrivate();
		sock->writePacket(serialized);

		// optional packages

		// PackageID | ModID | Enabled
		if (!database.query(std::string("SELECT * FROM optionalpackages WHERE Enabled = 1;"))) {
			std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		enabledResults = database.getResults<std::vector<std::string>>();
		if (userID != -1) {
			if (!database.query(std::string("SELECT * FROM optionalpackages WHERE Enabled = 0;"))) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			auto disabledResults = database.getResults<std::vector<std::string>>();
			if (!database.query("PREPARE selectEarlyUnlockStmt FROM \"SELECT * FROM earlyUnlocks WHERE ModID = ? AND UserID = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			for (auto vec : disabledResults) {
				if (!database.query("SET @paramModID=" + vec[1] + ";")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					return;
				}
				if (!database.query("EXECUTE selectEarlyUnlockStmt USING @paramModID, @paramUserID;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					return;
				}
				auto allowed = database.getResults<std::vector<std::string>>();
				if (!allowed.empty()) {
					enabledResults.push_back(vec);
				}
			}
		}
		common::UpdatePackageListMessage uplm;
		if (!database.query("PREPARE selectOptionalNameStmt FROM \"SELECT CAST(Name AS BINARY) FROM optionalpackagenames WHERE PackageID = ? AND Language = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}

		for (auto vec : enabledResults) {
			common::UpdatePackageListMessage::Package package;
			package.packageID = std::stoi(vec[0]);
			package.modID = std::stoi(vec[1]);
			if (!database.query("SET @paramPackageID=" + vec[0] + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("EXECUTE selectOptionalNameStmt USING @paramPackageID, @paramLanguage;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			auto results = database.getResults<std::vector<std::string>>();
			if (results.empty() || results[0].empty()) {
				continue;
			}
			package.name = results[0][0];
			package.downloadSize = _downloadSizeChecker->getBytesForPackage(package.modID, package.packageID, msg->language, versions[package.modID]);
			uplm.packages.push_back(package);
		}
		serialized = uplm.SerializePrivate();
		sock->writePacket(serialized);
	}

	void Server::handleModFilesListRequest(clockUtils::sockets::TcpSocket * sock, common::RequestModFilesMessage * msg) const {
		MariaDBWrapper database;
		if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
			std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		{
			if (!database.query("PREPARE selectStmt FROM \"SELECT Path, Hash FROM modfiles WHERE ModID = ? AND (Language = ? OR Language = 'All')\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("PREPARE selectVersionStmt FROM \"SELECT MajorVersion, MinorVersion, PatchVersion FROM mods WHERE ModID = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("PREPARE selectFileserverStmt FROM \"SELECT Url FROM fileserver WHERE ModID = ? AND MajorVersion = ? AND MinorVersion = ? AND PatchVersion = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramModID=" + std::to_string(msg->modID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramLanguage='" + msg->language + "';")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("EXECUTE selectStmt USING @paramModID, @paramLanguage;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			auto lastResults = database.getResults<std::vector<std::string>>();
			common::ListModFilesMessage lmfm;
			for (auto vec : lastResults) {
				lmfm.fileList.emplace_back(vec[0], vec[1]);
			}
			if (!database.query("EXECUTE selectVersionStmt USING @paramModID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			lastResults = database.getResults<std::vector<std::string>>();
			if (lastResults.empty()) {
				return;
			}
			const std::string majorVersion = lastResults[0][0];
			const std::string minorVersion = lastResults[0][1];
			const std::string patchVersion = lastResults[0][2];
			if (!database.query("SET @paramMajorVersion=" + majorVersion + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramMinorVersion=" + minorVersion + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramPatchVersion=" + patchVersion + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("EXECUTE selectFileserverStmt USING @paramModID, @paramMajorVersion, @paramMinorVersion, @paramPatchVersion;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			lastResults = database.getResults<std::vector<std::string>>();
			std::vector<std::string> possibilities = { DEFAULTURL };
			for (auto vec : lastResults) {
				possibilities.push_back(vec[0]);
			}
			lmfm.fileserver = possibilities[std::rand() % possibilities.size()];
			const std::string serialized = lmfm.SerializePrivate();
			sock->writePacket(serialized);
		}
	}

	void Server::handleDownloadSucceeded(clockUtils::sockets::TcpSocket *, common::DownloadSucceededMessage * msg) const {
		MariaDBWrapper database;
		if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
			std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		{
			if (!database.query("PREPARE insertStmt FROM \"INSERT INTO downloads (ModID, Counter) VALUES (?, 1) ON DUPLICATE KEY UPDATE Counter = Counter + 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramModID=" + std::to_string(msg->modID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("EXECUTE insertStmt USING @paramModID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			if (!database.query("PREPARE insertPerVersionStmt FROM \"INSERT INTO downloadsPerVersion (ModID, Version, Counter) VALUES (?, CONVERT(? USING BINARY), 1) ON DUPLICATE KEY UPDATE Counter = Counter + 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("PREPARE selectVersionStmt FROM \"SELECT MajorVersion, MinorVersion, PatchVersion FROM mods WHERE ModID = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("EXECUTE selectVersionStmt USING @paramModID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			auto results = database.getResults<std::vector<std::string>>();
			if (!results.empty()) {
				const std::string version = results[0][0] + "." + results[0][1] + "." + results[0][2];
				if (!database.query("SET @paramVersion='" + version + "';")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					return;
				}
				if (!database.query("EXECUTE insertPerVersionStmt USING @paramModID, @paramVersion;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					return;
				}
			}
		}
	}

	void Server::handleUpdatePlaytime(clockUtils::sockets::TcpSocket *, common::UpdatePlayTimeMessage * msg) const {
		MariaDBWrapper database;
		if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
			std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		const int userID = getUserID(msg->username, msg->password); // if userID is -1 user is not in database, so it's the play time of all unregistered players summed up
		if (!database.query("PREPARE insertStmt FROM \"INSERT INTO playtimes (ModID, UserID, Duration) VALUES (?, ?, ?) ON DUPLICATE KEY UPDATE Duration = Duration + ?\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE insertSessionTimeStmt FROM \"INSERT INTO sessionTimes (ModID, Duration) VALUES (?, ?)\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE deleteSessionInfosStmt FROM \"DELETE FROM userSessionInfos WHERE UserID = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE deleteSettingsStmt FROM \"DELETE FROM userSettings WHERE UserID = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("SET @paramModID=" + std::to_string(msg->modID) + ";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("SET @paramDuration=" + std::to_string(msg->duration) + ";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("EXECUTE insertStmt USING @paramModID, @paramUserID, @paramDuration, @paramDuration;")) {
			std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		if (!database.query("EXECUTE insertSessionTimeStmt USING @paramModID, @paramDuration;")) {
			std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		if (!database.query("PREPARE insertLastPlaytimeStmt FROM \"INSERT INTO lastPlayTimes (ModID, UserID, Timestamp) VALUES (?, ?, ?) ON DUPLICATE KEY UPDATE Timestamp = ?\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("SET @paramModID=" + std::to_string(msg->modID) + ";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		const int timestamp = std::chrono::duration_cast<std::chrono::hours>(std::chrono::system_clock::now() - std::chrono::system_clock::time_point()).count();
		if (!database.query("SET @paramTimestamp=" + std::to_string(timestamp) + ";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("EXECUTE insertLastPlaytimeStmt USING @paramModID, @paramUserID, @paramTimestamp, @paramTimestamp;")) {
			std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		if (!database.query("EXECUTE deleteSessionInfosStmt USING @paramUserID;")) {
			std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		if (!database.query("EXECUTE deleteSettingsStmt USING @paramUserID;")) {
			std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
	}

	void Server::handleRequestPlaytime(clockUtils::sockets::TcpSocket * sock, common::RequestPlayTimeMessage * msg) const {
		MariaDBWrapper database;
		if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
			std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		const int userID = getUserID(msg->username, msg->password);
		if (userID != -1) {
			if (!database.query("PREPARE selectStmt FROM \"SELECT Duration FROM playtimes WHERE ModID = ? AND UserID = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramModID=" + std::to_string(msg->modID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("EXECUTE selectStmt USING @paramModID, @paramUserID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			auto lastResults = database.getResults<std::vector<std::string>>();
			common::SendPlayTimeMessage sptm;
			if (!lastResults.empty()) {
				sptm.duration = std::stoi(lastResults[0][0]);
			}
			const std::string serialized = sptm.SerializePrivate();
			sock->writePacket(serialized);
		}
	}

	void Server::handleRequestScores(clockUtils::sockets::TcpSocket * sock, common::RequestScoresMessage * msg) const {
		MariaDBWrapper database;
		if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
			std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		if (!database.query("PREPARE selectStmt FROM \"SELECT Identifier, UserID, Score FROM modScores WHERE ModID = ? ORDER BY Score DESC\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("SET @paramModID=" + std::to_string(msg->modID) + ";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("EXECUTE selectStmt USING @paramModID;")) {
			std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		auto lastResults = database.getResults<std::vector<std::string>>();
		common::SendScoresMessage ssm;
		std::map<int, std::vector<std::pair<std::string, int32_t>>> scores;
		for (auto vec : lastResults) {
			int32_t identifier = int32_t(std::stoi(vec[0]));
			const int userID = std::stoi(vec[1]);
			int32_t score = int32_t(std::stoi(vec[2]));
			std::string username = getUsername(userID);
			if (!username.empty()) {
				scores[identifier].push_back(std::make_pair(username, score));
			}
		}
		for (auto & score : scores) {
			ssm.scores.emplace_back(score.first, score.second);
		}
		const std::string serialized = ssm.SerializePrivate();
		sock->writePacket(serialized);
	}

	void Server::handleUpdateScore(clockUtils::sockets::TcpSocket *, common::UpdateScoreMessage * msg) const {
		MariaDBWrapper database;
		if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
			std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		const int userID = getUserID(msg->username, msg->password);
		if (userID != -1) {
			if (!database.query("PREPARE selectStmt FROM \"SELECT * FROM modScoreList WHERE ModID = ? AND Identifier = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("PREPARE updateStmt FROM \"INSERT INTO modScores (ModID, UserID, Identifier, Score) VALUES (?, ?, ?, ?) ON DUPLICATE KEY UPDATE Score = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("PREPARE selectCheaterStmt FROM \"SELECT UserID FROM cheaters WHERE UserID = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramModID=" + std::to_string(msg->modID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramIdentifier=" + std::to_string(msg->identifier) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("EXECUTE selectCheaterStmt USING @paramUserID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			auto lastResults = database.getResults<std::vector<std::string>>();
			if (!lastResults.empty()) {
				return;
			}
			if (!database.query("EXECUTE selectStmt USING @paramModID, @paramIdentifier;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			lastResults = database.getResults<std::vector<std::string>>();
			if (!lastResults.empty()) {
				if (!database.query("SET @paramScore=" + std::to_string(msg->score) + ";")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					return;
				}
				if (!database.query("EXECUTE updateStmt USING @paramModID, @paramUserID, @paramIdentifier, @paramScore, @paramScore;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					return;
				}
			}
		}
	}

	void Server::handleRequestAchievements(clockUtils::sockets::TcpSocket * sock, common::RequestAchievementsMessage * msg) const {
		MariaDBWrapper database;
		if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
			std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		const int userID = getUserID(msg->username, msg->password);
		if (userID != -1) {
			if (!database.query("PREPARE selectStmt FROM \"SELECT Identifier FROM modAchievements WHERE ModID = ? AND UserID = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("PREPARE selectProgressMaxStmt FROM \"SELECT Identifier, Max FROM modAchievementProgressMax WHERE ModID = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("PREPARE selectProgressStmt FROM \"SELECT Current FROM modAchievementProgress WHERE ModID = ? AND UserID = ? AND Identifier = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramModID=" + std::to_string(msg->modID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("EXECUTE selectStmt USING @paramModID, @paramUserID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			auto lastResults = database.getResults<std::vector<std::string>>();
			common::SendAchievementsMessage sam;
			for (auto vec : lastResults) {
				int32_t identifier = int32_t(std::stoi(vec[0]));
				sam.achievements.push_back(identifier);
			}
			if (!database.query("EXECUTE selectProgressMaxStmt USING @paramModID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			lastResults = database.getResults<std::vector<std::string>>();
			for (auto vec : lastResults) {
				if (!database.query("SET @paramIdentifier=" + vec[0] + ";")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					return;
				}
				if (!database.query("EXECUTE selectProgressStmt USING @paramModID, @paramUserID, @paramIdentifier;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					return;
				}
				auto result = database.getResults<std::vector<std::string>>();
				if (result.empty()) {
					sam.achievementProgress.emplace_back(std::stoi(vec[0]), std::make_pair(0, std::stoi(vec[1])));
				} else {
					sam.achievementProgress.emplace_back(std::stoi(vec[0]), std::make_pair(std::stoi(result[0][0]), std::stoi(vec[1])));
				}
			}
			const std::string serialized = sam.SerializePrivate();
			sock->writePacket(serialized);
		}
	}

	void Server::handleUnlockAchievement(clockUtils::sockets::TcpSocket *, common::UnlockAchievementMessage * msg) const {
		MariaDBWrapper database;
		if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
			std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		const int userID = getUserID(msg->username, msg->password);
		if (userID != -1) {
			if (!database.query("PREPARE updateStmt FROM \"INSERT INTO modAchievements (ModID, UserID, Identifier) VALUES (?, ?, ?)\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("PREPARE selectStmt FROM \"SELECT * FROM modAchievementList WHERE ModID = ? AND Identifier = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("PREPARE selectPlaytimeStmt FROM \"SELECT Duration FROM playtimes WHERE ModID = ? AND UserID = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("PREPARE insertAchievementTimeStmt FROM \"INSERT INTO achievementTimes (ModID, Identifier, Duration) VALUES (?, ?, ?)\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramModID=" + std::to_string(msg->modID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramIdentifier=" + std::to_string(msg->identifier) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("EXECUTE selectStmt USING @paramModID, @paramIdentifier;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			auto lastResults = database.getResults<std::vector<std::string>>();
			if (!lastResults.empty()) {
				if (!database.query("EXECUTE updateStmt USING @paramModID, @paramUserID, @paramIdentifier;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					return;
				}
			}
			if (!database.query("EXECUTE selectPlaytimeStmt USING @paramModID, @paramUserID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			lastResults = database.getResults<std::vector<std::string>>();
			uint32_t duration = 0;
			if (!lastResults.empty()) {
				duration = uint32_t(std::stoi(lastResults[0][0]));
			}
			if (!database.query("SET @paramDuration=" + std::to_string(msg->duration + duration) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("EXECUTE insertAchievementTimeStmt USING @paramModID, @paramIdentifier, @paramDuration;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
		}
	}

	void Server::handleModVersionCheck(clockUtils::sockets::TcpSocket * sock, common::ModVersionCheckMessage * msg) const {
		MariaDBWrapper database;
		if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
			std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		if (!database.query("PREPARE selectModStmt FROM \"SELECT MajorVersion, MinorVersion, PatchVersion, Enabled FROM mods WHERE ModID = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectFileserverStmt FROM \"SELECT Url FROM fileserver WHERE ModID = ? AND MajorVersion = ? AND MinorVersion = ? AND PatchVersion = ?\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectEarlyStmt FROM \"SELECT * FROM earlyUnlocks WHERE ModID = ? AND UserID = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectFilesStmt FROM \"SELECT Path, Hash FROM modfiles WHERE ModID = ? AND (Language = ? OR Language = 'All')\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectPackagesStmt FROM \"SELECT PackageID FROM optionalpackages WHERE ModID = ?\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectPackageFilesStmt FROM \"SELECT Path, Hash FROM optionalpackagefiles WHERE PackageID = ? AND (Language = ? OR Language = 'All')\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectNameStmt FROM \"SELECT CAST(Name AS BINARY) FROM modnames WHERE ModID = ? AND Language = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		common::SendModsToUpdateMessage smtum;
		for (const common::ModVersion & mv : msg->modVersions) {
			if (!database.query("SET @paramModID=" + std::to_string(mv.modID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("EXECUTE selectModStmt USING @paramModID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			auto lastResults = database.getResults<std::vector<std::string>>();
			if (!lastResults.empty()) {
				const int8_t majorVersion = int8_t(std::stoi(lastResults[0][0]));
				const int8_t minorVersion = int8_t(std::stoi(lastResults[0][1]));
				const int8_t patchVersion = int8_t(std::stoi(lastResults[0][2]));
				bool enabled = std::stoi(lastResults[0][3]) == 1;

				if (!enabled) {
					const int userID = getUserID(msg->username, msg->password);
					if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
						std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
						break;
					}
					if (!database.query("EXECUTE selectEarlyStmt USING @paramModID, @paramUserID;")) {
						std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
						break;
					}
					auto lastEarlyResults = database.getResults<std::vector<std::string>>();
					enabled = !lastEarlyResults.empty();
				}

				if ((mv.majorVersion != majorVersion || mv.minorVersion != minorVersion || mv.patchVersion != patchVersion) && enabled) {
					if (!database.query("SET @paramLanguage='" + msg->language + "';")) {
						std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
						break;
					}
					if (!database.query("EXECUTE selectNameStmt USING @paramModID, @paramLanguage;")) {
						std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
						break;
					}
					auto lastNameResults = database.getResults<std::vector<std::string>>();
					std::string name;
					if (!lastNameResults.empty()) {
						name = lastNameResults[0][0];
					}
					common::ModUpdate mu;
					mu.modID = mv.modID;
					mu.name = name;
					mu.majorVersion = majorVersion;
					mu.minorVersion = minorVersion;
					mu.patchVersion = patchVersion;

					if (!database.query("EXECUTE selectFilesStmt USING @paramModID, @paramLanguage;")) {
						std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
						break;
					}
					auto lastFilesResults = database.getResults<std::vector<std::string>>();
					for (auto vec : lastFilesResults) {
						mu.files.emplace_back(vec[0], vec[1]);
					}

					if (!database.query("EXECUTE selectPackagesStmt USING @paramModID;")) {
						std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
						break;
					}
					lastFilesResults = database.getResults<std::vector<std::string>>();
					std::map<int, std::vector<std::pair<std::string, std::string>>> packageFiles;
					for (auto vec : lastFilesResults) {
						if (!database.query("SET @paramPackageID=" + vec[0] + ";")) {
							std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
							break;
						}
						if (!database.query("EXECUTE selectPackageFilesStmt USING @paramPackageID, @paramLanguage;")) {
							std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
							break;
						}
						auto pfs = database.getResults<std::vector<std::string>>();
						for (auto vec2 : pfs) {
							packageFiles[std::stoi(vec[0])].push_back(std::make_pair(vec2[0], vec2[1]));
						}
					}
					
					for (auto & packageFile : packageFiles) {
						mu.packageFiles.emplace_back(packageFile.first, packageFile.second);
					}
					if (!database.query("SET @paramMajorVersion=" + std::to_string(majorVersion) + ";")) {
						std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
						return;
					}
					if (!database.query("SET @paramMinorVersion=" + std::to_string(minorVersion) + ";")) {
						std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
						return;
					}
					if (!database.query("SET @paramPatchVersion=" + std::to_string(patchVersion) + ";")) {
						std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
						return;
					}
					if (!database.query("EXECUTE selectFileserverStmt USING @paramModID, @paramMajorVersion, @paramMinorVersion, @paramPatchVersion;")) {
						std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
						return;
					}
					lastResults = database.getResults<std::vector<std::string>>();
					std::vector<std::string> possibilities = { DEFAULTURL };
					for (auto vec : lastResults) {
						possibilities.push_back(vec[0]);
					}
					mu.fileserver = possibilities[std::rand() % possibilities.size()];
					smtum.updates.push_back(mu);
				}
			}
		}
		const std::string serialized = smtum.SerializePrivate();
		sock->writePacket(serialized);
	}

	void Server::handleFeedback(clockUtils::sockets::TcpSocket * sock, common::FeedbackMessage * msg) const {
		std::stringstream ss;
		ss << "reports/feedback_" << sock->getRemoteIP() << "_" << time(nullptr) << ".txt";
		if (!boost::filesystem::exists("reports")) {
			if (!boost::filesystem::create_directory("reports")) {
			}
		}
		std::fstream file;
		file.open(ss.str(), std::fstream::out);
		file << msg->text << std::endl << std::endl << int(msg->majorVersion) << "." << int(msg->minorVersion) << "." << int(msg->patchVersion) << std::endl << std::endl << msg->username;
		file.close();
		sendMail("New Feedback arrived", ss.str() + "\n" + msg->text + "\n" + std::to_string(int(msg->majorVersion)) + "." + std::to_string(int(msg->minorVersion)) + "." + std::to_string(int(msg->patchVersion)) + "\n\n" + msg->username);
	}

	void Server::handleRequestOriginalFiles(clockUtils::sockets::TcpSocket * sock, common::RequestOriginalFilesMessage * msg) const {
		MariaDBWrapper database;
		if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
			std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		if (!database.query("PREPARE selectStmt FROM \"SELECT ModID, Path, Hash FROM modfiles WHERE ModID = ? AND Path = ?\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		common::SendOriginalFilesMessage sofm;
		for (const auto p : msg->files) {
			if (!database.query("SET @paramModID=" + std::to_string(p.first) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramFile='" + p.second + "';")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("EXECUTE selectStmt USING @paramModID, @paramFile;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			auto lastResults = database.getResults<std::vector<std::string>>();
			if (lastResults.empty()) {
				if (!database.query("SET @paramFile='" + p.second + ".z';")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("EXECUTE selectStmt USING @paramModID, @paramFile;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
				lastResults = database.getResults<std::vector<std::string>>();
			}
			if (!lastResults.empty()) {
				int32_t modID = int32_t(std::stoi(lastResults[0][0]));
				std::string file = lastResults[0][1];
				std::string hash = lastResults[0][2];
				sofm.files.emplace_back(modID, std::make_pair(file, hash));
			}
		}
		const std::string serialized = sofm.SerializePrivate();
		sock->writePacket(serialized);
	}

	void Server::handleUpdateLoginTime(clockUtils::sockets::TcpSocket *, common::UpdateLoginTimeMessage * msg) const {
		MariaDBWrapper database;
		if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
			std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		const int userID = getUserID(msg->username, msg->password);
		if (userID != -1) {
			if (!database.query("PREPARE insertStmt FROM \"INSERT INTO lastLoginTimes (UserID, Timestamp) VALUES (?, ?) ON DUPLICATE KEY UPDATE Timestamp = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			const int timestamp = std::chrono::duration_cast<std::chrono::hours>(std::chrono::system_clock::now() - std::chrono::system_clock::time_point()).count();
			if (!database.query("SET @paramTimestamp=" + std::to_string(timestamp) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("EXECUTE insertStmt USING @paramUserID, @paramTimestamp, @paramTimestamp;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
		}
	}

	void Server::handleRequestPackageFiles(clockUtils::sockets::TcpSocket * sock, common::RequestPackageFilesMessage * msg) const {
		MariaDBWrapper database;
		if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
			std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		{
			if (!database.query("PREPARE selectStmt FROM \"SELECT Path, Hash FROM optionalpackagefiles WHERE PackageID = ? AND (Language = ? OR Language = 'All')\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("PREPARE selectModIDStmt FROM \"SELECT ModID FROM optionalpackages WHERE PackageID = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("PREPARE selectVersionStmt FROM \"SELECT MajorVersion, MinorVersion, PatchVersion FROM mods WHERE ModID = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("PREPARE selectFileserverStmt FROM \"SELECT Url FROM fileserver WHERE ModID = ? AND MajorVersion = ? AND MinorVersion = ? AND PatchVersion = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramPackageID=" + std::to_string(msg->packageID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramLanguage='" + msg->language + "';")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("EXECUTE selectStmt USING @paramPackageID, @paramLanguage;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			auto lastResults = database.getResults<std::vector<std::string>>();
			common::ListModFilesMessage lmfm;
			for (auto vec : lastResults) {
				lmfm.fileList.emplace_back(vec[0], vec[1]);
			}
			if (!database.query("EXECUTE selectModIDStmt USING @paramPackageID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			lastResults = database.getResults<std::vector<std::string>>();
			if (lastResults.empty()) {
				return;
			}
			if (!database.query("SET @paramModID=" + lastResults[0][0] + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("EXECUTE selectVersionStmt USING @paramModID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			lastResults = database.getResults<std::vector<std::string>>();
			if (lastResults.empty()) {
				return;
			}
			const std::string majorVersion = lastResults[0][0];
			const std::string minorVersion = lastResults[0][1];
			const std::string patchVersion = lastResults[0][2];
			if (!database.query("SET @paramMajorVersion=" + majorVersion + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramMinorVersion=" + minorVersion + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramPatchVersion=" + patchVersion + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("EXECUTE selectFileserverStmt USING @paramModID, @paramMajorVersion, @paramMinorVersion, @paramPatchVersion;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			lastResults = database.getResults<std::vector<std::string>>();
			std::vector<std::string> possibilities = { DEFAULTURL };
			for (auto vec : lastResults) {
				possibilities.push_back(vec[0]);
			}
			lmfm.fileserver = possibilities[std::rand() % possibilities.size()];
			const std::string serialized = lmfm.SerializePrivate();
			sock->writePacket(serialized);
		}
	}

	void Server::handlePackageDownloadSucceeded(clockUtils::sockets::TcpSocket *, common::PackageDownloadSucceededMessage * msg) const {
		MariaDBWrapper database;
		if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
			std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		{
			if (!database.query("PREPARE insertStmt FROM \"INSERT INTO packagedownloads (PackageID, Counter) VALUES (?, 1) ON DUPLICATE KEY UPDATE Counter = Counter + 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramPackageID=" + std::to_string(msg->packageID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("EXECUTE insertStmt USING @paramPackageID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
		}
	}

	void Server::handleRequestAllModStats(clockUtils::sockets::TcpSocket * sock, common::RequestAllModStatsMessage * msg) const {
		MariaDBWrapper database;
		if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
			std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		const int userID = getUserID(msg->username, msg->password);
		if (userID != -1) {
			if (!database.query("PREPARE selectPlayedModsStmt FROM \"SELECT ModID, Duration FROM playtimes WHERE UserID = ? ORDER BY Duration DESC\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("PREPARE selectModNameStmt FROM \"SELECT CAST(Name AS BINARY) FROM modnames WHERE ModID = ? AND Language = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("PREPARE selectLastTimePlayedStmt FROM \"SELECT Timestamp FROM lastPlayTimes WHERE ModID = ? AND UserID = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("PREPARE selectAchievementsStmt FROM \"SELECT IFNULL(COUNT(*), 0) FROM modAchievements WHERE ModID = ? AND UserID = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("PREPARE selectAllAchievementsStmt FROM \"SELECT IFNULL(COUNT(*), 0) FROM modAchievementList WHERE ModID = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("PREPARE selectScoreStmt FROM \"SELECT Score, Identifier, UserID FROM modScores WHERE ModID = ? ORDER BY Score DESC\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("PREPARE selectScoreNameStmt FROM \"SELECT CAST(Name AS BINARY) FROM modScoreNames WHERE ModID = ? AND Identifier = ? AND Language = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("PREPARE selectModTypeStmt FROM \"SELECT Type FROM mods WHERE ModID = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramLanguage='" + msg->language + "';")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("EXECUTE selectPlayedModsStmt USING @paramUserID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			auto lastResults = database.getResults<std::vector<std::string>>();
			common::SendAllModStatsMessage samsm;
			for (auto vec : lastResults) {
				// get mod name in current language
				common::ModStats ms;
				ms.modID = std::stoi(vec[0]);
				ms.duration = std::stoi(vec[1]);

				if (!database.query("SET @paramModID=" + vec[0] + ";")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					continue;
				}
				if (!database.query("EXECUTE selectModNameStmt USING @paramModID, @paramLanguage;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					continue;
				}
				auto results = database.getResults<std::vector<std::string>>();
				if (results.empty()) {
					continue;
				}
				ms.name = results[0][0];
				if (!database.query("EXECUTE selectModTypeStmt USING @paramModID;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					continue;
				}
				results = database.getResults<std::vector<std::string>>();
				if (results.empty()) {
					continue;
				}
				ms.type = common::ModType(std::stoi(results[0][0]));
				if (!database.query("EXECUTE selectLastTimePlayedStmt USING @paramModID, @paramUserID;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					continue;
				}
				results = database.getResults<std::vector<std::string>>();
				if (results.empty()) {
					ms.lastTimePlayed = -1;
				} else {
					ms.lastTimePlayed = std::stoi(results[0][0]);
				}
				if (!database.query("EXECUTE selectAllAchievementsStmt USING @paramModID;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					continue;
				}
				results = database.getResults<std::vector<std::string>>();
				if (results.empty()) {
					ms.achievedAchievements = 0;
					ms.allAchievements = 0;
				} else {
					ms.allAchievements = std::stoi(results[0][0]);
					if (!database.query("EXECUTE selectAchievementsStmt USING @paramModID, @paramUserID;")) {
						std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
						continue;
					}
					results = database.getResults<std::vector<std::string>>();
					if (results.empty()) {
						ms.achievedAchievements = 0;
					} else {
						ms.achievedAchievements = std::stoi(results[0][0]);
					}
				}
				if (!database.query("EXECUTE selectScoreStmt USING @paramModID;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					continue;
				}
				results = database.getResults<std::vector<std::string>>();
				if (results.empty()) {
					ms.bestScore = 0;
					ms.bestScoreName = "";
					ms.bestScoreRank = -1;
				} else {
					std::map<int, std::vector<std::pair<int, int>>> scores;
					for (auto s : results) {
						scores[std::stoi(s[1])].push_back(std::make_pair(std::stoi(s[0]), std::stoi(s[2])));
					}
					ms.bestScore = 0;
					ms.bestScoreName = "";
					ms.bestScoreRank = 0;
					int identifier = -1;
					for (auto & score : scores) {
						int rank = 1;
						int lastRank = 1;
						int lastScore = 0;
						for (const auto p : score.second) {
							if (lastScore != p.first) {
								rank = lastRank;
							}
							if (p.second == userID) {
								if (rank < ms.bestScoreRank || ms.bestScoreRank == 0 || (ms.bestScoreRank == rank && ms.bestScore < p.first)) {
									ms.bestScore = p.first;
									ms.bestScoreRank = rank;
									identifier = score.first;
									break;
								}
							}
							lastScore = p.first;
							lastRank++;
						}
					}
					if (identifier != -1) {
						if (!database.query("SET @paramIdentifier=" + std::to_string(identifier) + ";")) {
							std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
							continue;
						}
						if (!database.query("EXECUTE selectScoreNameStmt USING @paramModID, @paramIdentifier, @paramLanguage;")) {
							std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
							continue;
						}
						results = database.getResults<std::vector<std::string>>();
						if (results.empty()) {
							ms.bestScore = 0;
							ms.bestScoreRank = 0;
							ms.bestScoreName = "";
						} else {
							ms.bestScoreName = results[0][0];
						}
					}
				}
				samsm.mods.push_back(ms);
			}

			const std::string serialized = samsm.SerializePrivate();
			sock->writePacket(serialized);
		}
	}

	void Server::handleRequestSingleModStat(clockUtils::sockets::TcpSocket * sock, common::RequestSingleModStatMessage * msg) const {
		MariaDBWrapper database;
		if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
			std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		const int userID = getUserID(msg->username, msg->password);
		if (!database.query("PREPARE selectLastTimePlayedStmt FROM \"SELECT Timestamp FROM lastPlayTimes WHERE ModID = ? AND UserID = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectTimePlayedStmt FROM \"SELECT Duration FROM playtimes WHERE ModID = ? AND UserID = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectAchievementsStmt FROM \"SELECT IFNULL(COUNT(*), 0) FROM modAchievements WHERE ModID = ? AND UserID = ?\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectAllAchievementsStmt FROM \"SELECT IFNULL(COUNT(*), 0) FROM modAchievementList WHERE ModID = ?\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectScoreStmt FROM \"SELECT Score, Identifier, UserID FROM modScores WHERE ModID = ? ORDER BY Score DESC\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectScoreNameStmt FROM \"SELECT CAST(Name AS BINARY) FROM modScoreNames WHERE ModID = ? AND Identifier = ? AND Language = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("SET @paramLanguage='" + msg->language + "';")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("SET @paramModID=" + std::to_string(msg->modID) + ";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		common::ModStats ms;
		if (userID != -1) {
			if (!database.query("EXECUTE selectLastTimePlayedStmt USING @paramModID, @paramUserID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			auto lastResults = database.getResults<std::vector<std::string>>();
			if (lastResults.empty()) {
				ms.lastTimePlayed = -1;
			} else {
				ms.lastTimePlayed = std::stoi(lastResults[0][0]);
			}
		} else {
			ms.lastTimePlayed = -1;
		}
		// get mod name in current language
		ms.modID = msg->modID;

		if (!database.query("EXECUTE selectTimePlayedStmt USING @paramModID, @paramUserID;")) {
			std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
		}
		auto results = database.getResults<std::vector<std::string>>();
		if (!results.empty() && userID != -1) {
			ms.duration = std::stoi(results[0][0]);
		} else {
			ms.duration = 0;
		}
		if (!database.query("EXECUTE selectAllAchievementsStmt USING @paramModID;")) {
			std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
		}
		results = database.getResults<std::vector<std::string>>();
		if (results.empty()) {
			ms.achievedAchievements = 0;
			ms.allAchievements = 0;
		} else {
			ms.allAchievements = std::stoi(results[0][0]);
			if (!database.query("EXECUTE selectAchievementsStmt USING @paramModID, @paramUserID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			}
			results = database.getResults<std::vector<std::string>>();
			if (results.empty()) {
				ms.achievedAchievements = 0;
			} else {
				ms.achievedAchievements = std::stoi(results[0][0]);
			}
		}
		if (!database.query("EXECUTE selectScoreStmt USING @paramModID;")) {
			std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
		}
		results = database.getResults<std::vector<std::string>>();
		if (results.empty()) {
			ms.bestScore = 0;
			ms.bestScoreName = "";
			ms.bestScoreRank = -1;
		} else {
			std::map<int, std::vector<std::pair<int, int>>> scores;
			for (auto s : results) {
				scores[std::stoi(s[1])].push_back(std::make_pair(std::stoi(s[0]), std::stoi(s[2])));
			}
			ms.bestScore = 0;
			ms.bestScoreName = "";
			ms.bestScoreRank = 0;
			int identifier = -1;
			for (auto & score : scores) {
				int realRank = 1;
				int lastScore = 0;
				int rank = 1;
				for (const auto p : score.second) {
					if (lastScore != p.first) {
						rank = realRank;
					}
					if (p.second == userID) {
						if (rank < ms.bestScoreRank || ms.bestScoreRank == 0 || (ms.bestScoreRank == rank && ms.bestScore < p.first)) {
							ms.bestScore = p.first;
							ms.bestScoreRank = rank;
							identifier = score.first;
							break;
						}
					}
					lastScore = p.first;
					realRank++;
				}
			}
			if (identifier != -1) {
				if (!database.query("SET @paramIdentifier=" + std::to_string(identifier) + ";")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				}
				if (!database.query("EXECUTE selectScoreNameStmt USING @paramModID, @paramIdentifier, @paramLanguage;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				}
				results = database.getResults<std::vector<std::string>>();
				if (results.empty()) {
					ms.bestScore = 0;
					ms.bestScoreRank = 0;
					ms.bestScoreName = "";
				} else {
					ms.bestScoreName = results[0][0];
				}
			}
		}

		common::SendSingleModStatMessage ssmsm;
		ssmsm.mod = ms;
		const std::string serialized = ssmsm.SerializePrivate();
		sock->writePacket(serialized);
	}

	void Server::handleRequestAllAchievementStats(clockUtils::sockets::TcpSocket * sock, common::RequestAllAchievementStatsMessage * msg) const {
		MariaDBWrapper database;
		if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
			std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		const int userID = getUserID(msg->username, msg->password);
		if (!database.query("PREPARE selectAllAchievementsStmt FROM \"SELECT Identifier FROM modAchievementList WHERE ModID = ?\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectPlayerCountStmt FROM \"SELECT IFNULL(COUNT(*), 0) FROM playtimes WHERE ModID = ? AND UserID != -1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectAchievementNameStmt FROM \"SELECT CAST(Name AS BINARY) FROM modAchievementNames WHERE ModID = ? AND Identifier = ? AND Language = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectAchievementDescriptionStmt FROM \"SELECT CAST(Description AS BINARY) FROM modAchievementDescriptions WHERE ModID = ? AND Identifier = ? AND Language = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectAchievementIconsStmt FROM \"SELECT LockedIcon, LockedHash, UnlockedIcon, UnlockedHash FROM modAchievementIcons WHERE ModID = ? AND Identifier = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectAchievementHiddenStmt FROM \"SELECT * FROM modAchievementHidden WHERE ModID = ? AND Identifier = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectUnlockedAchievementsStmt FROM \"SELECT IFNULL(COUNT(*), 0) FROM modAchievements WHERE ModID = ? AND Identifier = ?\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectAllOwnAchievementsStmt FROM \"SELECT Identifier FROM modAchievements WHERE ModID = ? AND UserID = ? AND Identifier = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectAchievementProgressStmt FROM \"SELECT Current FROM modAchievementProgress WHERE ModID = ? AND UserID = ? AND Identifier = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectAchievementMaxProgressStmt FROM \"SELECT Max FROM modAchievementProgressMax WHERE ModID = ? AND Identifier = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("SET @paramLanguage='" + msg->language + "';")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("SET @paramModID=" + std::to_string(msg->modID) + ";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("EXECUTE selectAllAchievementsStmt USING @paramModID;")) {
			std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		auto lastResults = database.getResults<std::vector<std::string>>();
		if (!database.query("EXECUTE selectPlayerCountStmt USING @paramModID;")) {
			std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		auto playerResult = database.getResults<std::vector<std::string>>();
		int playerCount = 0;
		if (!playerResult.empty()) {
			playerCount = std::stoi(playerResult[0][0]);
		}
		common::SendAllAchievementStatsMessage saasm;
		for (auto vec : lastResults) {
			// get mod name in current language
			common::SendAllAchievementStatsMessage::AchievementStats as;
			if (!database.query("SET @paramIdentifier=" + vec[0] + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				continue;
			}
			if (!database.query("EXECUTE selectAchievementNameStmt USING @paramModID, @paramIdentifier, @paramLanguage;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				continue;
			}
			auto results = database.getResults<std::vector<std::string>>();
			if (results.empty()) {
				continue;
			}
			as.name = results[0][0];
			if (!database.query("EXECUTE selectAchievementDescriptionStmt USING @paramModID, @paramIdentifier, @paramLanguage;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				continue;
			}
			results = database.getResults<std::vector<std::string>>();
			if (!results.empty()) {
				as.description = results[0][0];
			}
			if (!database.query("EXECUTE selectAchievementIconsStmt USING @paramModID, @paramIdentifier;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				continue;
			}
			results = database.getResults<std::vector<std::string>>();
			if (!results.empty()) {
				as.iconLocked = results[0][0];
				as.iconLockedHash = results[0][1];
				as.iconUnlocked = results[0][2];
				as.iconUnlockedHash = results[0][3];
			}
			if (!database.query("EXECUTE selectAchievementHiddenStmt USING @paramModID, @paramIdentifier;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				continue;
			}
			results = database.getResults<std::vector<std::string>>();
			as.hidden = !results.empty();
			if (!database.query("EXECUTE selectUnlockedAchievementsStmt USING @paramModID, @paramIdentifier;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				continue;
			}
			results = database.getResults<std::vector<std::string>>();
			if (results.empty()) {
				as.unlockedPercent = 0.0;
			} else {
				as.unlockedPercent = (playerCount == 0) ? 0.0 : double(std::stoi(results[0][0]) * 100) / playerCount;
			}
			if (!database.query("EXECUTE selectAllOwnAchievementsStmt USING @paramModID, @paramUserID, @paramIdentifier;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				continue;
			}
			results = database.getResults<std::vector<std::string>>();
			as.unlocked = !results.empty();
			if (!database.query("EXECUTE selectAchievementMaxProgressStmt USING @paramModID, @paramIdentifier;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				continue;
			}
			results = database.getResults<std::vector<std::string>>();
			if (results.empty()) {
				as.maxProgress = 0;
			} else {
				as.maxProgress = std::stoi(results[0][0]);
			}
			if (!database.query("EXECUTE selectAchievementProgressStmt USING @paramModID, @paramUserID, @paramIdentifier;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				continue;
			}
			results = database.getResults<std::vector<std::string>>();
			if (results.empty()) {
				as.currentProgress = 0;
			} else {
				as.currentProgress = std::stoi(results[0][0]);
			}
			saasm.achievements.push_back(as);
		}

		const std::string serialized = saasm.SerializePrivate();
		sock->writePacket(serialized);
	}

	void Server::handleRequestAllScoreStats(clockUtils::sockets::TcpSocket * sock, common::RequestAllScoreStatsMessage * msg) const {
		MariaDBWrapper database;
		if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
			std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		const int userID = getUserID(msg->username, msg->password);
		if (!database.query("PREPARE selectScoreStmt FROM \"SELECT Score, UserID FROM modScores WHERE ModID = ? AND Identifier = ? ORDER BY Score DESC\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectScoreNameStmt FROM \"SELECT CAST(Name AS BINARY), Identifier FROM modScoreNames WHERE ModID = ? AND Language = ? ORDER BY Identifier ASC\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("SET @paramLanguage='" + msg->language + "';")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("SET @paramModID=" + std::to_string(msg->modID) + ";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("EXECUTE selectScoreNameStmt USING @paramModID, @paramLanguage;")) {
			std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		auto lastResults = database.getResults<std::vector<std::string>>();
		common::SendAllScoreStatsMessage sassm;
		for (auto vec : lastResults) {
			common::SendAllScoreStatsMessage::ScoreStats ss;
			ss.name = vec[0];

			if (!database.query("SET @paramIdentifier=" + vec[1] + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				continue;
			}
			if (!database.query("EXECUTE selectScoreStmt USING @paramModID, @paramIdentifier;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				continue;
			}
			auto results = database.getResults<std::vector<std::string>>();
			for (auto score : results) {
				ss.scores.emplace_back(getUsername(std::stoi(score[1])), std::stoi(score[0]));
			}
			sassm.scores.push_back(ss);
		}

		const std::string serialized = sassm.SerializePrivate();
		sock->writePacket(serialized);
	}

	void Server::handleRequestAllNews(clockUtils::sockets::TcpSocket * sock, common::RequestAllNewsMessage * msg) const {
		MariaDBWrapper database;
		if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
			std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		if (!database.query("PREPARE selectNews FROM \"SELECT NewsID, CAST(Title AS BINARY), CAST(Body AS BINARY), Timestamp FROM news WHERE Language = ? AND NewsID > ?\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectModRefsStmt FROM \"SELECT ModID FROM newsModReferences WHERE NewsID = ?\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectImagesStmt FROM \"SELECT File, Hash FROM newsImageReferences WHERE NewsID = ?\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectModnameStmt FROM \"SELECT CAST(Name AS BINARY) FROM modnames WHERE ModID = ? AND Language = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("SET @paramLanguage='" + msg->language + "';")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("SET @paramLastNewsID=" + std::to_string(msg->lastNewsID) + ";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("EXECUTE selectNews USING @paramLanguage, @paramLastNewsID;")) {
			std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		auto lastResults = database.getResults<std::vector<std::string>>();
		common::SendAllNewsMessage sanm;
		for (auto vec : lastResults) {
			common::SendAllNewsMessage::News news;
			news.id = std::stoi(vec[0]);
			news.title = vec[1];
			news.body = vec[2];
			news.timestamp = std::stoi(vec[3]);

			if (!database.query("SET @paramNewsID=" + vec[0] + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				continue;
			}
			if (!database.query("EXECUTE selectModRefsStmt USING @paramNewsID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				continue;
			}
			auto results = database.getResults<std::vector<std::string>>();
			for (auto modID : results) {
				if (!database.query("SET @paramModID=" + modID[0] + ";")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					continue;
				}
				if (!database.query("EXECUTE selectModnameStmt USING @paramModID, @paramLanguage;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					continue;
				}
				auto modname = database.getResults<std::vector<std::string>>();
				if (!modname.empty()) {
					news.referencedMods.emplace_back(std::stoi(modID[0]), modname[0][0]);
				}
			}
			if (!database.query("EXECUTE selectImagesStmt USING @paramNewsID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				continue;
			}
			results = database.getResults<std::vector<std::string>>();
			for (auto p : results) {
				news.imageFiles.emplace_back(p[0], p[1]);
			}
			sanm.news.push_back(news);
		}

		const std::string serialized = sanm.SerializePrivate();
		sock->writePacket(serialized);
	}

	void Server::handleSubmitNews(clockUtils::sockets::TcpSocket *, common::SubmitNewsMessage * msg) const {
		const int userID = getUserID(msg->username, msg->password);
		MariaDBWrapper database;
		if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
			std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		if (!database.query("PREPARE selectNewsWriterStmt FROM \"SELECT UserID FROM newsWriter WHERE UserID = ? AND Language = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("SET @paramLanguage='" + msg->language + "';")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("EXECUTE selectNewsWriterStmt USING @paramUserID, @paramLanguage;")) {
			std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		auto results = database.getResults<std::vector<std::string>>();
		if (results.empty()) {
			const std::string newsDir = std::to_string(time(nullptr));
			if (!boost::filesystem::exists("news")) {
				if (!boost::filesystem::create_directory("news")) {
				}
			}
			if (!boost::filesystem::exists("news/" + newsDir)) {
				if (!boost::filesystem::create_directory("news/" + newsDir)) {
				}
			}
			std::stringstream ss;
			ss << "news/" << newsDir << "/news.txt";
			std::fstream file;
			file.open(ss.str(), std::fstream::out);
			file << msg->news.title << std::endl << std::endl;
			file << msg->news.timestamp << std::endl << std::endl;
			file << msg->news.body << std::endl << std::endl;
			for (int mod : msg->mods) {
				file << mod << std::endl;
			}
			file << std::endl;
			for (const std::pair<std::string, std::string> & image : msg->news.imageFiles) {
				file << image.first << std::endl;
			}
			file.close();

			for (size_t i = 0; i < msg->images.size(); i++) {
				std::ofstream out;
				out.open("news/" + newsDir + "/" + msg->news.imageFiles[i].first, std::ios::out | std::ios::binary);
				out.write(reinterpret_cast<char *>(&msg->images[i][0]), msg->images[i].size());
				out.close();
			}

			sendMail("New News arrived", ss.str() + "\n" + msg->news.title + "\n" + msg->news.body);
		} else {
			if (msg->images.size() != msg->news.imageFiles.size()) {
				std::cout << "Images-Count unequal: " << msg->images.size() << " vs. " << msg->news.imageFiles.size() << std::endl;
				return;
			}
			std::lock_guard<std::mutex> lg(_newsLock);
			if (!database.query("PREPARE selectNewsIDStmt FROM \"SELECT NewsID FROM news ORDER BY NewsID DESC LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("PREPARE selectImageStmt FROM \"SELECT Hash FROM newsImageReferences WHERE File = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("PREPARE insertModRefsStmt FROM \"INSERT INTO newsModReferences (NewsID, ModID) VALUES (?, ?)\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("PREPARE insertImageRefsStmt FROM \"INSERT INTO newsImageReferences (NewsID, File, Hash) VALUES (?, ?, ?)\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("PREPARE insertNewsStmt FROM \"INSERT INTO news (Title, Body, Timestamp, Language) VALUES (CONVERT(? USING BINARY), CONVERT(? USING BINARY), ?, ?)\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("EXECUTE selectNewsIDStmt;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			results = database.getResults<std::vector<std::string>>();
			int newsID;
			if (!results.empty()) {
				newsID = std::stoi(results[0][0]) + 1;
			} else {
				return;
			}
			if (!database.query("SET @paramNewsID=" + std::to_string(newsID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			for (size_t i = 0; i < msg->news.imageFiles.size(); i++) {
				if (!database.query("SET @paramFile='" + msg->news.imageFiles[i].first + "';")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					return;
				}
				if (!database.query("EXECUTE selectImageStmt USING @paramFile;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					return;
				}
				results = database.getResults<std::vector<std::string>>();
				std::string hash = msg->news.imageFiles[i].second;
				if (results.empty()) {
					if (msg->images[i].empty()) {
						return;
					}
					std::ofstream out;
					out.open("/var/www/vhosts/clockwork-origins.de/httpdocs/Gothic/downloads/news/images/" + msg->news.imageFiles[i].first, std::ios::out | std::ios::binary);
					out.write(reinterpret_cast<char *>(&msg->images[i][0]), msg->images[i].size());
					out.close();
				} else {
					hash = results[0][0];
				}
				if (!database.query("SET @paramHash='" + hash + "';")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					return;
				}
				if (!database.query("EXECUTE insertImageRefsStmt USING @paramNewsID, @paramFile, @paramHash;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					return;
				}
			}
			for (int modID : msg->mods) {
				if (!database.query("SET @paramModID=" + std::to_string(modID) + ";")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					return;
				}
				if (!database.query("EXECUTE insertModRefsStmt USING @paramNewsID, @paramModID;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					return;
				}
			}
			if (!database.query("SET @paramTitle='" + msg->news.title + "';")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramBody='" + msg->news.body + "';")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramTimestamp=" + std::to_string(msg->news.timestamp) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("EXECUTE insertNewsStmt USING @paramTitle, @paramBody, @paramTimestamp, @paramLanguage;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
		}
	}

	void Server::handleLinkClicked(clockUtils::sockets::TcpSocket *, common::LinkClickedMessage * msg) const {
		MariaDBWrapper database;
		if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
			std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		if (!database.query("PREPARE insertLinkClick FROM \"INSERT INTO linksClicked (NewsID, Url, Counter) VALUES (?, ?, 1) ON DUPLICATE KEY UPDATE Counter = Counter + 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("SET @paramNewsID=" + std::to_string(msg->newsID) + ";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("SET @paramUrl='" + msg->url + "';")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("EXECUTE insertLinkClick USING @paramNewsID, @paramUrl;")) {
			std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
	}

	void Server::handleSubmitScriptFeatures(clockUtils::sockets::TcpSocket *, common::SubmitScriptFeaturesMessage * msg) const {
		MariaDBWrapper database;
		do {
			const int userID = getUserID(msg->username, msg->password);
			if (userID == -1) {
				break;
			}
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("PREPARE selectTeamIDStmt FROM \"SELECT TeamID FROM mods WHERE ModID = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE checkUserPermission FROM \"SELECT UserID FROM teammembers WHERE UserID = ? AND TeamID = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE insertScoreStmt FROM \"INSERT IGNORE INTO modScoreList (ModID, Identifier) VALUES (?, ?)\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE insertScoreNameStmt FROM \"INSERT INTO modScoreNames (ModID, Identifier, Name, Language) VALUES (?, ?, CONVERT(? USING BINARY), ?) ON DUPLICATE KEY UPDATE Name = CONVERT(? USING BINARY)\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE insertAchievementStmt FROM \"INSERT IGNORE INTO modAchievementList (ModID, Identifier) VALUES (?, ?)\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE insertAchievementNameStmt FROM \"INSERT INTO modAchievementNames (ModID, Identifier, Name, Language) VALUES (?, ?, CONVERT(? USING BINARY), ?) ON DUPLICATE KEY UPDATE Name = CONVERT(? USING BINARY)\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE insertAchievementDescriptionStmt FROM \"INSERT INTO modAchievementDescriptions (ModID, Identifier, Description, Language) VALUES (?, ?, CONVERT(? USING BINARY), ?) ON DUPLICATE KEY UPDATE Description = CONVERT(? USING BINARY)\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE insertAchievementHiddenStmt FROM \"INSERT IGNORE INTO modAchievementHidden (ModID, Identifier) VALUES (?, ?)\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE deleteAchievementHiddenStmt FROM \"DELETE FROM modAchievementHidden WHERE ModID = ? AND Identifier = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE insertAchievementProgressStmt FROM \"INSERT INTO modAchievementProgressMax (ModID, Identifier, Max) VALUES (?, ?, ?) ON DUPLICATE KEY UPDATE Max = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE deleteAchievementProgressStmt FROM \"DELETE FROM modAchievementProgressMax WHERE ModID = ? AND Identifier = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectLockedHashStmt FROM \"SELECT LockedHash FROM modAchievementIcons WHERE ModID = ? AND LockedImage = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectUnlockedHashStmt FROM \"SELECT UnlockedHash FROM modAchievementIcons WHERE ModID = ? AND UnlockedImage = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE insertAchievementIconsStmt FROM \"INSERT INTO modAchievementIcons (ModID, Identifier, LockedImage, LockedHash, UnlockedImage, UnlockedHash) VALUES (?, ?, ?, ?, ?, ?) ON DUPLICATE KEY UPDATE LockedImage = ?, LockedHash = ?, UnlockedImage = ?, UnlockedHash = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramLanguage='" + msg->language + "';")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramModID=" + std::to_string(msg->modID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("EXECUTE selectTeamIDStmt USING @paramModID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			auto lastResults = database.getResults<std::vector<std::string>>();
			if (lastResults.empty()) {
				break;
			}
			if (!database.query("EXECUTE checkUserPermission USING @paramUserID, @paramModID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			lastResults = database.getResults<std::vector<std::string>>();
			if (lastResults.empty()) {
				break;
			}
			for (size_t i = 0; i < msg->scores.size(); i++) {
				if (!database.query("SET @paramIdentifier=" + std::to_string(i) + ";")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("EXECUTE insertScoreStmt USING @paramModID, @paramIdentifier;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
				if (!database.query("SET @paramScoreName='" + msg->scores[i].name + "';")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("EXECUTE insertScoreNameStmt USING @paramModID, @paramIdentifier, @paramScoreName, @paramLanguage, @paramScoreName;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
			}
			for (size_t i = 0; i < msg->achievements.size(); i++) {
				if (!database.query("SET @paramIdentifier=" + std::to_string(i) + ";")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("EXECUTE insertAchievementStmt USING @paramModID, @paramIdentifier;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
				if (!database.query("SET @paramAchievementName='" + msg->achievements[i].name + "';")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("EXECUTE insertAchievementNameStmt USING @paramModID, @paramIdentifier, @paramAchievementName, @paramLanguage, @paramAchievementName;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
				if (!database.query("SET @paramAchievementDescription='" + msg->achievements[i].description + "';")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("EXECUTE insertAchievementDescriptionStmt USING @paramModID, @paramIdentifier, @paramAchievementDescription, @paramLanguage, @paramAchievementDescription;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
				if (msg->achievements[i].hidden) {
					if (!database.query("EXECUTE insertAchievementHiddenStmt USING @paramModID, @paramIdentifier;")) {
						std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
						break;
					}
				} else {
					if (!database.query("EXECUTE deleteAchievementHiddenStmt USING @paramModID, @paramIdentifier;")) {
						std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
						break;
					}
				}
				if (msg->achievements[i].maxProgress > 0) {
					if (!database.query("SET @paramAchievementProgress=" + std::to_string(msg->achievements[i].maxProgress) + ";")) {
						std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
						break;
					}
					if (!database.query("EXECUTE insertAchievementProgressStmt USING @paramModID, @paramIdentifier, @paramAchievementProgress, @paramAchievementProgress;")) {
						std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
						break;
					}
				} else {
					if (!database.query("EXECUTE deleteAchievementProgressStmt USING @paramModID, @paramIdentifier;")) {
						std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
						break;
					}
				}
				std::string lockedHash;
				if (!msg->achievements[i].lockedImageName.empty()) {
					for (auto it = msg->achievementImages.begin(); it != msg->achievementImages.end(); ++it) {
						if (it->first.first == msg->achievements[i].lockedImageName) {
							lockedHash = it->first.second;
							break;
						}
					}
					if (lockedHash.empty()) {
						if (!database.query("SET @paramImage='" + msg->achievements[i].lockedImageName + "';")) {
							std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
							break;
						}
						if (!database.query("EXECUTE selectLockedHashStmt USING @paramModID, @paramImage;")) {
							std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
							break;
						}
						auto hashVec = database.getResults<std::vector<std::string>>();
						if (hashVec.empty()) {
							if (!database.query("EXECUTE selectUnlockedHashStmt USING @paramModID, @paramImage;")) {
								std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
								break;
							}
							hashVec = database.getResults<std::vector<std::string>>();
						}
						if (!hashVec.empty()) {
							lockedHash = hashVec[0][0];
						}
					}
				}
				std::string unlockedHash;
				if (!msg->achievements[i].unlockedImageName.empty()) {
					for (auto it = msg->achievementImages.begin(); it != msg->achievementImages.end(); ++it) {
						if (it->first.first == msg->achievements[i].unlockedImageName) {
							unlockedHash = it->first.second;
							break;
						}
					}
					if (unlockedHash.empty()) {
						if (!database.query("SET @paramImage='" + msg->achievements[i].unlockedImageName + "';")) {
							std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
							break;
						}
						if (!database.query("EXECUTE selectLockedHashStmt USING @paramModID, @paramImage;")) {
							std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
							break;
						}
						auto hashVec = database.getResults<std::vector<std::string>>();
						if (hashVec.empty()) {
							if (!database.query("EXECUTE selectUnlockedHashStmt USING @paramModID, @paramImage;")) {
								std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
								break;
							}
							hashVec = database.getResults<std::vector<std::string>>();
						}
						if (!hashVec.empty()) {
							unlockedHash = hashVec[0][0];
						}
					}
				}
				if (!database.query("SET @paramLockedImage='" + msg->achievements[i].lockedImageName + "';")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("SET @paramLockedHash='" + lockedHash + "';")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("SET @paramUnlockedImage='" + msg->achievements[i].unlockedImageName + "';")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("SET @paramUnlockedHash='" + unlockedHash + "';")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("EXECUTE insertAchievementIconsStmt USING @paramModID, @paramIdentifier, @paramLockedImage, @paramLockedHash, @paramUnlockedImage, @paramUnlockedHash, @paramLockedImage, @paramLockedHash, @paramUnlockedImage, @paramUnlockedHash;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
			}
			boost::filesystem::create_directories("/var/www/vhosts/clockwork-origins.de/httpdocs/Gothic/downloads/mods/" + std::to_string(msg->modID) + "/achievements");
			for (auto p : msg->achievementImages) {
				std::ofstream out;
				out.open("/var/www/vhosts/clockwork-origins.de/httpdocs/Gothic/downloads/mods/" + std::to_string(msg->modID) + "/achievements/" + p.first.first, std::ios::out | std::ios::binary);
				out.write(reinterpret_cast<char *>(&p.second[0]), p.second.size());
				out.close();
			}
		} while (false);

		sendMail("New Script Features arrived", std::to_string(msg->modID) + "\n" + msg->username + "\n" + std::to_string(msg->achievements.size()) + " Achievements\n" + std::to_string(msg->scores.size()) + " Scores");
	}

	void Server::handleRequestInfoPage(clockUtils::sockets::TcpSocket * sock, common::RequestInfoPageMessage * msg) const {
		MariaDBWrapper database;
		if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
			std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		if (!database.query("PREPARE selectModnameStmt FROM \"SELECT CAST(Name AS BINARY) FROM modnames WHERE ModID = ? AND Language = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectScreensStmt FROM \"SELECT Image, Hash FROM screens WHERE ModID = ?\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectDescriptionStmt FROM \"SELECT CAST(Description AS BINARY) FROM descriptions WHERE ModID = ? AND Language = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectFeaturesStmt FROM \"SELECT DISTINCT CAST(Feature AS BINARY) FROM features WHERE ModID = ? AND Language = ?\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectSpineFeaturesStmt FROM \"SELECT Features FROM spinefeatures WHERE ModID = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectTeamIDStmt FROM \"SELECT TeamID FROM mods WHERE ModID = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectTeamStmt FROM \"SELECT * FROM teammembers WHERE TeamID = ? AND UserID = ?\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectEditRightsStmt FROM \"SELECT * FROM editrights WHERE ModID = ? AND UserID = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectModEnabledStmt FROM \"SELECT Enabled FROM mods WHERE ModID = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectEarlyAccessStmt FROM \"SELECT * FROM earlyUnlocks WHERE ModID = ? AND UserID = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectOptionalPackagesStmt FROM \"SELECT PackageID FROM optionalpackages WHERE ModID = ? AND Enabled = 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectOptionalPackageNameStmt FROM \"SELECT CAST(Name AS BINARY) FROM optionalpackagenames WHERE PackageID = ? AND Language = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("PREPARE selectVersionStmt FROM \"SELECT MajorVersion, MinorVersion, PatchVersion FROM mods WHERE ModID = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("SET @paramLanguage='" + msg->language + "';")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("SET @paramModID=" + std::to_string(msg->modID) + ";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return;
		}
		if (!database.query("EXECUTE selectModnameStmt USING @paramModID, @paramLanguage;")) {
			std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
			return;
		}
		auto lastResults = database.getResults<std::vector<std::string>>();
		common::SendInfoPageMessage sipm;
		if (!lastResults.empty()) {
			sipm.modname = lastResults[0][0];
			if (!database.query("EXECUTE selectScreensStmt USING @paramModID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			lastResults = database.getResults<std::vector<std::string>>();
			for (auto p : lastResults) {
				sipm.screenshots.emplace_back(p[0], p[1]);
			}
			if (!database.query("EXECUTE selectDescriptionStmt USING @paramModID, @paramLanguage;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			lastResults = database.getResults<std::vector<std::string>>();
			if (!lastResults.empty()) {
				sipm.description = lastResults[0][0];
			}
			if (!database.query("EXECUTE selectFeaturesStmt USING @paramModID, @paramLanguage;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			lastResults = database.getResults<std::vector<std::string>>();
			for (auto v : lastResults) {
				sipm.features.push_back(v[0]);
			}
			if (!database.query("EXECUTE selectSpineFeaturesStmt USING @paramModID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			lastResults = database.getResults<std::vector<std::string>>();
			if (!lastResults.empty()) {
				sipm.spineFeatures = std::stoi(lastResults[0][0]);
			}
			if (!database.query("EXECUTE selectTeamIDStmt USING @paramModID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			lastResults = database.getResults<std::vector<std::string>>();
			const int userID = getUserID(msg->username, msg->password);
			if (!database.query("SET @paramTeamID=" + lastResults[0][0] + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("EXECUTE selectTeamStmt USING @paramTeamID, @paramUserID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			lastResults = database.getResults<std::vector<std::string>>();
			sipm.editRights = !lastResults.empty();
			if (!database.query("EXECUTE selectEditRightsStmt USING @paramModID, @paramUserID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			lastResults = database.getResults<std::vector<std::string>>();
			sipm.editRights |= !lastResults.empty();
			if (!database.query("EXECUTE selectVersionStmt USING @paramModID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			lastResults = database.getResults<std::vector<std::string>>();
			if (lastResults.empty()) {
				return;
			}
			sipm.majorVersion = uint8_t(std::stoi(lastResults[0][0]));
			sipm.minorVersion = uint8_t(std::stoi(lastResults[0][1]));
			sipm.patchVersion = uint8_t(std::stoi(lastResults[0][2]));
			if (!database.query("EXECUTE selectModEnabledStmt USING @paramModID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			lastResults = database.getResults<std::vector<std::string>>();
			sipm.installAllowed = !lastResults.empty() && std::stoi(lastResults[0][0]) == 1;
			if (!sipm.installAllowed) {
				if (!database.query("EXECUTE selectEarlyAccessStmt USING @paramModID, @paramUserID;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					return;
				}
				lastResults = database.getResults<std::vector<std::string>>();
				sipm.installAllowed = !lastResults.empty();
			}
			if (sipm.installAllowed) {
				if (!database.query("EXECUTE selectOptionalPackagesStmt USING @paramModID;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					return;
				}
				lastResults = database.getResults<std::vector<std::string>>();
				for (auto vec : lastResults) {
					if (!database.query("SET @paramPackageID=" + vec[0] + ";")) {
						std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
						return;
					}
					if (!database.query("EXECUTE selectOptionalPackageNameStmt USING @paramPackageID, @paramLanguage;")) {
						std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
						return;
					}
					auto results = database.getResults<std::vector<std::string>>();
					if (!results.empty()) {
						sipm.optionalPackages.emplace_back(std::stoi(vec[0]), results[0][0]);
					}
				}
			}
		}
		const std::string serialized = sipm.SerializePrivate();
		sock->writePacket(serialized);
	}

	void Server::handleSubmitInfoPage(clockUtils::sockets::TcpSocket * sock, common::SubmitInfoPageMessage * msg) const {
		MariaDBWrapper database;
		do {
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("PREPARE updateDescriptionStmt FROM \"INSERT INTO descriptions (ModID, Description, Language) VALUES (?, CONVERT(? USING BINARY), ?) ON DUPLICATE KEY UPDATE Description = CONVERT(? USING BINARY)\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE removeFeaturesStmt FROM \"DELETE FROM features WHERE ModID = ? AND Language = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE insertFeatureStmt FROM \"INSERT INTO features (ModID, Feature, Language) VALUES (?, CONVERT(? USING BINARY), ?)\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE updateSpineFeaturesStmt FROM \"INSERT INTO spinefeatures (ModID, Features) VALUES (?, ?) ON DUPLICATE KEY UPDATE Features = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectScreenStmt FROM \"SELECT Image, Hash FROM screens WHERE ModID = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE deleteScreenStmt FROM \"DELETE FROM screens WHERE ModID = ? AND Image = ? AND Hash = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE insertScreenStmt FROM \"INSERT INTO screens (ModID, Image, Hash) VALUES (?, ?, ?)\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramLanguage='" + msg->language + "';")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramModID=" + std::to_string(msg->modID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramDescription='" + msg->description + "';")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("EXECUTE updateDescriptionStmt USING @paramModID, @paramDescription, @paramLanguage, @paramDescription;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("EXECUTE removeFeaturesStmt USING @paramModID, @paramLanguage;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			for (const std::string & s : msg->features) {
				if (!database.query("SET @paramFeature='" + s + "';")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("EXECUTE insertFeatureStmt USING @paramModID, @paramFeature, @paramLanguage;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
			}
			if (!database.query("SET @paramModules=" + std::to_string(msg->spineFeatures) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("EXECUTE updateSpineFeaturesStmt USING @paramModID, @paramModules, @paramModules;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("EXECUTE selectScreenStmt USING @paramModID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			auto lastResults = database.getResults<std::vector<std::string>>();
			std::vector<std::pair<std::string, std::string>> images;
			for (auto v : lastResults) {
				images.emplace_back(v[0], v[1]);
			}
			// compare images with received ones to delete old screens
			for (const auto p : images) {
				bool found = false;
				for (const auto p2 : msg->screenshots) {
					if (p.first == p2.first && p.second == p2.second) {
						// identical image, so skip
						found = true;
						break;
					}
				}
				if (!found) { // remove deleted image
					if (!database.query("SET @paramImage='" + p.first + "';")) {
						std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
						break;
					}
					if (!database.query("SET @paramHash='" + p.second + "';")) {
						std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
						break;
					}
					if (!database.query("EXECUTE deleteScreenStmt USING @paramModID, @paramImage, @paramHash;")) {
						std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
						break;
					}
					boost::filesystem::remove("/var/www/vhosts/clockwork-origins.de/httpdocs/Gothic/downloads/mods/" + std::to_string(msg->modID) + "/screens/" + p.first);
				}
			}
			for (const auto p : msg->screenshots) {
				bool found = false;
				for (const auto p2 : images) {
					if (p.first == p2.first && p.second == p2.second) {
						// identical image, so skip
						found = true;
						break;
					}
				}
				if (!found) { // remove deleted image
					if (!database.query("SET @paramImage='" + p.first + "';")) {
						std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
						break;
					}
					if (!database.query("SET @paramHash='" + p.second + "';")) {
						std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
						break;
					}
					if (!database.query("EXECUTE insertScreenStmt USING @paramModID, @paramImage, @paramHash;")) {
						std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
						break;
					}
					boost::filesystem::create_directories("/var/www/vhosts/clockwork-origins.de/httpdocs/Gothic/downloads/mods/" + std::to_string(msg->modID) + "/screens/"); // ensure folder exists
				}
			}
			for (auto p : msg->imageFiles) {
				std::ofstream out;
				out.open("/var/www/vhosts/clockwork-origins.de/httpdocs/Gothic/downloads/mods/" + std::to_string(msg->modID) + "/screens/" + p.first, std::ios::out | std::ios::binary);
				out.write(reinterpret_cast<char *>(&p.second[0]), p.second.size());
				out.close();
			}
		} while (false);
		// send ack
		common::AckMessage ack;
		const std::string serialized = ack.SerializePrivate();
		sock->writePacket(serialized);
	}

	void Server::handleSendUserInfos(clockUtils::sockets::TcpSocket * sock, common::SendUserInfosMessage * msg) const {
		const int userID = getUserID(msg->username, msg->password);
		if (userID != -1) {
			MariaDBWrapper database;
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			if (!database.query("PREPARE insertHashStmt FROM \"INSERT IGNORE INTO userHashes (UserID, Hash) VALUES (?, ?)\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("PREPARE updateSessionInfosStmt FROM \"INSERT INTO userSessionInfos (UserID, Mac, IP, Hash) VALUES (?, ?, ?, ?) ON DUPLICATE KEY UPDATE Mac = ?, IP = ?, Hash = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("PREPARE updateUserSettingsStmt FROM \"INSERT INTO userSettings (UserID, Entry, Value) VALUES (?, ?, ?) ON DUPLICATE KEY UPDATE Value = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("PREPARE updateUserLanguageStmt FROM \"INSERT INTO userLanguages (UserID, Language) VALUES (?, ?) ON DUPLICATE KEY UPDATE Language = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("PREPARE insertBanStmt FROM \"INSERT IGNORE INTO provisoricalBans (UserID) VALUES (?)\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("PREPARE selectSessionInfoMatchStmt FROM \"SELECT * FROM userSessionInfos WHERE UserID = ? AND Mac = ? AND Hash = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramHash='" + msg->hash + "';")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramMac='" + msg->mac + "';")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramIP='" + sock->getRemoteIP() + "';")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramLanguage='" + msg->language + "';")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				return;
			}
			if (msg->ownID == 0) { // in this case settings have to empty
				if (!msg->settings.empty()) {
					if (!database.query("EXECUTE insertBanStmt USING @paramUserID;")) {
						std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
						return;
					}
					return;
				}
				if (!database.query("EXECUTE updateUserLanguageStmt USING @paramUserID, @paramLanguage, @paramLanguage;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					return;
				}
			} else {
				if (msg->settings.empty()) { // if starting a mod, settings don't have to be empty
					if (!database.query("EXECUTE insertBanStmt USING @paramUserID;")) {
						std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
						return;
					}
					return;
				}
				if (!database.query("EXECUTE selectSessionInfoMatchStmt USING @paramUserID, @paramMac, @paramHash;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					return;
				}
				auto lastResults = database.getResults<std::vector<std::string>>();
				if (lastResults.empty()) { // if current mac doesn't match the mac from startup => ban
					if (!database.query("EXECUTE insertBanStmt USING @paramUserID;")) {
						std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
						return;
					}
					return;
				}
			}
			if (!database.query("EXECUTE insertHashStmt USING @paramUserID, @paramHash;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			if (!database.query("EXECUTE updateSessionInfosStmt USING @paramUserID, @paramMac, @paramIP, @paramHash, @paramMac, @paramIP, @paramHash;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				return;
			}
			for (const std::pair<std::string, std::string> p : msg->settings) {
				if (!database.query("SET @paramEntry='" + p.first + "';")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					continue;
				}
				if (!database.query("SET @paramValue='" + p.second + "';")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					continue;
				}
				if (!database.query("EXECUTE updateUserSettingsStmt USING @paramUserID, @paramEntry, @paramValue, @paramValue;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					continue;
				}
			}
		}
	}

	void Server::handleRequestRandomMod(clockUtils::sockets::TcpSocket * sock, common::RequestRandomModMessage * msg) const {
		common::SendRandomModMessage srmm;
		srmm.modID = -1;
		do {
			MariaDBWrapper database;
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("PREPARE selectSpineModStmt FROM \"SELECT ModID FROM spinefeatures WHERE Features != 0\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectLanguageStmt FROM \"SELECT ModID FROM modnames WHERE ModID = ? AND Language = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectRandomModStmt FROM \"SELECT ModID FROM modnames WHERE Language = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramLanguage='" + msg->language + "';")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("EXECUTE selectSpineModStmt;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			auto lastResults = database.getResults<std::vector<std::string>>();
			std::vector<int> ids;
			for (auto vec : lastResults) {
				if (!database.query("SET @paramModID=" + vec[0] + ";")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					continue;
				}
				if (!database.query("EXECUTE selectLanguageStmt USING @paramModID, @paramLanguage;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					continue;
				}
				auto results = database.getResults<std::vector<std::string>>();
				if (results.empty()) {
					continue;
				}
				ids.push_back(std::stoi(results[0][0]));
			}
			if (!ids.empty()) {
				srmm.modID = ids[std::rand() % ids.size()];
			}
			if (srmm.modID == -1) {
				if (!database.query("EXECUTE selectRandomModStmt USING @paramLanguage;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
				lastResults = database.getResults<std::vector<std::string>>();
				if (!lastResults.empty()) {
					srmm.modID = std::stoi(lastResults[std::rand() % lastResults.size()][0]);
				}
			}
		} while (false);
		const std::string serialized = srmm.SerializePrivate();
		sock->writePacket(serialized);
	}

	void Server::handleUpdateAchievementProgress(clockUtils::sockets::TcpSocket *, common::UpdateAchievementProgressMessage * msg) const {
		do {
			const int userID = getUserID(msg->username, msg->password);
			if (userID == -1) {
				break;
			}
			MariaDBWrapper database;
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("PREPARE updateProgressStmt FROM \"INSERT INTO modAchievementProgress (ModID, UserID, Identifier, Current) VALUES (?, ?, ?, ?) ON DUPLICATE KEY UPDATE Current = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramModID=" + std::to_string(msg->modID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramIdentifier=" + std::to_string(msg->identifier) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramProgress=" + std::to_string(msg->progress) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("EXECUTE updateProgressStmt USING @paramModID, @paramUserID, @paramIdentifier, @paramProgress, @paramProgress;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
		} while (false);
	}

	void Server::handleSubmitCompatibility(clockUtils::sockets::TcpSocket *, common::SubmitCompatibilityMessage * msg) const {
		do {
			const int userID = getUserID(msg->username, msg->password);
			if (userID == -1) {
				break;
			}
			MariaDBWrapper database;
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("PREPARE updateCompatibilityStmt FROM \"INSERT INTO compatibilityList (UserID, ModID, PatchID, Compatible) VALUES (?, ?, ?, ?) ON DUPLICATE KEY UPDATE Compatible = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramModID=" + std::to_string(msg->modID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramPatchID=" + std::to_string(msg->patchID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramCompatible=" + std::to_string(msg->compatible) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("EXECUTE updateCompatibilityStmt USING @paramUserID, @paramModID, @paramPatchID, @paramCompatible, @paramCompatible;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
		} while (false);
	}

	void Server::handleRequestOwnCompatibilities(clockUtils::sockets::TcpSocket * sock, common::RequestOwnCompatibilitiesMessage * msg) const {
		common::SendOwnCompatibilitiesMessage socm;
		do {
			const int userID = getUserID(msg->username, msg->password);
			if (userID == -1) {
				break;
			}
			MariaDBWrapper database;
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("PREPARE selectCompatibilitiesStmt FROM \"SELECT ModID, PatchID, Compatible FROM compatibilityList WHERE UserID = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("EXECUTE selectCompatibilitiesStmt USING @paramUserID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			auto lastResults = database.getResults<std::vector<std::string>>();
			for (auto vec : lastResults) {
				common::SendOwnCompatibilitiesMessage::Compatibility c {};
				c.modID = std::stoi(vec[0]);
				c.patchID = std::stoi(vec[1]);
				c.compatible = std::stoi(vec[2]) == 1;
				socm.compatibilities.push_back(c);
			}
		} while (false);
		const std::string serialized = socm.SerializePrivate();
		sock->writePacket(serialized);
	}

	void Server::handleRequestCompatibilityList(clockUtils::sockets::TcpSocket * sock, common::RequestCompatibilityListMessage * msg) const {
		common::SendCompatibilityListMessage sclm;
		do {
			MariaDBWrapper database;
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("PREPARE selectForbiddenStmt FROM \"SELECT DISTINCT PatchID FROM forbiddenPatches WHERE ModID = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectCompatibilitiesStmt FROM \"SELECT DISTINCT PatchID FROM compatibilityList WHERE ModID = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectCompatibilitiesValuesStmt FROM \"SELECT SUM(Compatible) AS UpVotes, COUNT(Compatible) AS Amount FROM compatibilityList WHERE ModID = ? AND PatchID = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramModID=" + std::to_string(msg->modID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("EXECUTE selectForbiddenStmt USING @paramModID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			auto lastResults = database.getResults<std::vector<std::string>>();
			for (auto vec : lastResults) {
				sclm.forbiddenPatches.push_back(std::stoi(vec[0]));
			}
			if (!database.query("EXECUTE selectCompatibilitiesStmt USING @paramModID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			lastResults = database.getResults<std::vector<std::string>>();
			for (auto vec : lastResults) {
				if (!database.query("SET @paramPatchID=" + vec[0] + ";")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("EXECUTE selectCompatibilitiesValuesStmt USING @paramModID, @paramPatchID;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
				auto results = database.getResults<std::vector<std::string>>();
				const int upVotes = std::stoi(results[0][0]);
				const int amount = std::stoi(results[0][1]);
				if (upVotes < amount / 2) { // if less then 50% of all users rated combination as compatible it is assumed to be incompatible
					sclm.impossiblePatches.push_back(std::stoi(vec[0]));
				}
			}
		} while (false);
		const std::string serialized = sclm.SerializePrivate();
		sock->writePacket(serialized);
	}

	void Server::handleRequestRating(clockUtils::sockets::TcpSocket * sock, common::RequestRatingMessage * msg) const {
		common::SendRatingMessage srm;
		srm.modID = msg->modID;
		srm.sum = 0;
		srm.allowedToRate = !msg->username.empty();
		do {
			const int userID = getUserID(msg->username, msg->password);
			MariaDBWrapper database;
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("PREPARE selectOwnRatingStmt FROM \"SELECT Rating FROM ratings WHERE ModID = ? AND UserID = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectRatingStmt FROM \"SELECT IFNULL(SUM(Rating), 0), IFNULL(COUNT(Rating), 1) FROM ratings WHERE ModID = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectOwnPlaytimeStmt FROM \"SELECT Duration FROM playtimes WHERE ModID = ? AND UserID = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectOverallPlaytimeStmt FROM \"SELECT IFNULL(SUM(Duration), 0), IFNULL(COUNT(Duration), 1) FROM playtimes WHERE ModID = ? AND UserID != -1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramModID=" + std::to_string(msg->modID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (userID == -1) {
				if (!database.query("EXECUTE selectRatingStmt USING @paramModID;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
				auto lastResults = database.getResults<std::vector<std::string>>();
				const int sum = std::stoi(lastResults[0][0]);
				const int count = std::stoi(lastResults[0][1]);
				srm.voteCount = count;
				srm.sum = sum;
			} else {
				if (!database.query("EXECUTE selectOwnRatingStmt USING @paramModID, @paramUserID;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
				auto lastResults = database.getResults<std::vector<std::string>>();
				if (!lastResults.empty()) {
					srm.sum = std::stoi(lastResults[0][0]);
				}
				srm.voteCount = 0;
				if (!database.query("EXECUTE selectOwnPlaytimeStmt USING @paramModID, @paramUserID;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
				lastResults = database.getResults<std::vector<std::string>>();
				int32_t duration = 0;
				if (!lastResults.empty()) {
					duration = std::stoi(lastResults[0][0]);
				}
				if (!database.query("EXECUTE selectOverallPlaytimeStmt USING @paramModID;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
				lastResults = database.getResults<std::vector<std::string>>();
				const int sum = std::stoi(lastResults[0][0]);
				int count = std::stoi(lastResults[0][1]);
				if (count == 0) {
					count = 1;
				}
				const int average = sum / count;
				srm.allowedToRate = duration >= average && count > 1;
			}
		} while (false);
		const std::string serialized = srm.SerializePrivate();
		sock->writePacket(serialized);
	}

	void Server::handleSubmitRating(clockUtils::sockets::TcpSocket *, common::SubmitRatingMessage * msg) const {
		do {
			const int userID = getUserID(msg->username, msg->password);
			if (userID == -1) {
				break;
			}
			MariaDBWrapper database;
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("PREPARE insertStmt FROM \"INSERT INTO ratings (ModID, UserID, Rating) VALUES (?, ?, ?) ON DUPLICATE KEY UPDATE Rating = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramModID=" + std::to_string(msg->modID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramRating=" + std::to_string(msg->rating) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("EXECUTE insertStmt USING @paramModID, @paramUserID, @paramRating, @paramRating;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
		} while (false);
	}

	void Server::handleAutoUpdateEncrypted(clockUtils::sockets::TcpSocket * sock, common::UpdateRequestEncryptedMessage * msg) const {
		// 1. read xml
		tinyxml2::XMLDocument doc;

		const tinyxml2::XMLError e = doc.LoadFile("Spine_Version.xml");

		if (e) {
			std::cerr << "Couldn't open xml file!" << std::endl;
			common::UpdateFileCountMessage ufcm;
			ufcm.count = 0;

			const std::string serialized = ufcm.SerializePrivate();

			sock->writePacket(serialized);
			return;
		}

		uint32_t lastVersion = (msg->majorVersion << 16) + (msg->minorVersion << 8) + msg->patchVersion;

		std::map<uint32_t, std::vector<std::pair<std::string, std::string>>> versions;
		versions.insert(std::make_pair(lastVersion, std::vector<std::pair<std::string, std::string>>()));

		for (tinyxml2::XMLElement * node = doc.FirstChildElement("Version"); node != nullptr; node = node->NextSiblingElement("Version")) {
			const uint8_t majorVersion = uint8_t(std::stoi(node->Attribute("majorVersion")));
			const uint8_t minorVersion = uint8_t(std::stoi(node->Attribute("minorVersion")));
			const uint8_t patchVersion = uint8_t(std::stoi(node->Attribute("patchVersion")));
			uint32_t version = (majorVersion << 16) + (minorVersion << 8) + patchVersion;

			versions.insert(std::make_pair(version, std::vector<std::pair<std::string, std::string>>()));

			auto it = versions.find(version);

			for (tinyxml2::XMLElement * file = node->FirstChildElement("File"); file != nullptr; file = file->NextSiblingElement("File")) {
				it->second.emplace_back(file->GetText(), file->Attribute("Hash"));
			}
		}

		// 2. find all changed files
		std::set<std::pair<std::string, std::string>> files;

		auto it = versions.find(lastVersion);
		if (it != versions.end()) {
			++it;
			for (; it != versions.end(); ++it) {
				for (std::pair<std::string, std::string> & file : it->second) {
					files.insert(file);
				}
				lastVersion = it->first;
			}
		}

		// 3. send file count
		common::UpdateFilesMessage ufm;
		for (auto & p : files) {
			ufm.files.push_back(p);
		}
		const std::string serialized = ufm.SerializePrivate();
		sock->writePacket(serialized);
	}

	void Server::handleRequestOverallSaveData(clockUtils::sockets::TcpSocket * sock, common::RequestOverallSaveDataMessage * msg) const {
		common::SendOverallSaveDataMessage som;
		do {
			const int userID = getUserID(msg->username, msg->password);
			if (userID == -1) {
				break;
			}
			MariaDBWrapper database;
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("PREPARE selectStmt FROM \"SELECT Entry, Value FROM overallSaveData WHERE UserID = ? AND ModID = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramModID=" + std::to_string(msg->modID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("EXECUTE selectStmt USING @paramUserID, @paramModID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			auto lastResults = database.getResults<std::vector<std::string>>();
			for (auto vec : lastResults) {
				if (vec.size() == 2) {
					som.data.emplace_back(vec[0], vec[1]);
				}
			}
		} while (false);
		const std::string serialized = som.SerializePrivate();
		sock->writePacket(serialized);
	}

	void Server::handleUpdateOverallSaveData(clockUtils::sockets::TcpSocket *, common::UpdateOverallSaveDataMessage * msg) const {
		do {
			const int userID = getUserID(msg->username, msg->password);
			if (userID == -1) {
				break;
			}
			MariaDBWrapper database;
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("PREPARE insertStmt FROM \"INSERT INTO overallSaveData (UserID, ModID, Entry, Value) VALUES (?, ?, ?, ?) ON DUPLICATE KEY UPDATE Value = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramModID=" + std::to_string(msg->modID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramEntry='" + msg->entry + "';")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramValue='" + msg->value + "';")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("EXECUTE insertStmt USING @paramUserID, @paramModID, @paramEntry, @paramValue, @paramValue;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
		} while (false);
	}

	void Server::handleRequestModManagement(clockUtils::sockets::TcpSocket * sock, common::RequestModManagementMessage * msg) const {
		common::SendModManagementMessage smmm;
		do {
			const int userID = getUserID(msg->username, msg->password);
			if (userID == -1) {
				break;
			}
			MariaDBWrapper database;
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("PREPARE selectTeamsStmt FROM \"SELECT TeamID FROM teammembers WHERE UserID = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectModStmt FROM \"SELECT ModID, MajorVersion, MinorVersion, PatchVersion, Enabled, Gothic, Type, ReleaseDate FROM mods WHERE TeamID = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectModNameStmt FROM \"SELECT CAST(Name AS BINARY) FROM modnames WHERE ModID = ? AND Language = CONVERT(? USING BINARY) LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectModFilesStmt FROM \"SELECT Path, Hash, Language FROM modfiles WHERE ModID = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectEarlyAccessorsStmt FROM \"SELECT UserID FROM earlyUnlocks WHERE ModID = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectDevDurationStmt FROM \"SELECT Duration FROM devtimes WHERE ModID = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectScoreNamesStmt FROM \"SELECT Identifier, CAST(Name AS BINARY), CAST(Language AS BINARY) FROM modScoreNames WHERE ModID = ? ORDER BY Identifier ASC\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectAchievementNamesStmt FROM \"SELECT Identifier, CAST(Name AS BINARY), CAST(Language AS BINARY) FROM modAchievementNames WHERE ModID = ? ORDER BY Identifier ASC\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectAchievementDescriptionStmt FROM \"SELECT CAST(Description AS BINARY) FROM modAchievementDescriptions WHERE ModID = ? AND Identifier = ? AND Language = CONVERT(? USING BINARY) LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectAchievementProgressStmt FROM \"SELECT Max FROM modAchievementProgressMax WHERE ModID = ? AND Identifier = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectAchievementHiddenStmt FROM \"SELECT ModID FROM modAchievementHidden WHERE ModID = ? AND Identifier = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectAchievementIconsStmt FROM \"SELECT LockedIcon, LockedHash, UnlockedIcon, UnlockedHash FROM modAchievementIcons WHERE ModID = ? AND Identifier = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectOverallDownloadsStmt FROM \"SELECT Counter FROM downloads WHERE ModID = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectDownloadsPerVersionStmt FROM \"SELECT Version, Counter FROM downloadsPerVersion WHERE ModID = ? ORDER BY Version ASC\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectPlaytimeStmt FROM \"SELECT Duration FROM playtimes WHERE ModID = ? AND UserID != -1 ORDER BY Duration ASC\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectSessiontimeStmt FROM \"SELECT Duration FROM sessionTimes WHERE ModID = ? ORDER BY Duration ASC\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectAchievementtimeStmt FROM \"SELECT Duration FROM achievementTimes WHERE ModID = ? AND Identifier = ? ORDER BY Duration ASC\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectIntervalPlayersStmt FROM \"SELECT COUNT(*) FROM lastPlayTimes WHERE ModID = ? AND UserID != -1 AND Timestamp > ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectChapterStatsStmt FROM \"SELECT Identifier, Guild, StatName, StatValue FROM chapterStats WHERE ModID = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramLanguage='" + msg->language + "';")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("EXECUTE selectTeamsStmt USING @paramUserID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			auto lastResults = database.getResults<std::vector<std::string>>();
			for (auto vec : lastResults) {
				if (!database.query("SET @paramTeamID=" + vec[0] + ";")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("EXECUTE selectModStmt USING @paramTeamID;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
				auto results = database.getResults<std::vector<std::string>>();
				for (auto mod : results) {
					if (!database.query("SET @paramModID=" + mod[0] + ";")) {
						std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
						break;
					}
					if (!database.query("SET @paramLanguage='" + msg->language + "';")) {
						std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
						break;
					}
					if (!database.query("EXECUTE selectModNameStmt USING @paramModID, @paramLanguage;")) {
						std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
						break;
					}
					auto nameResults = database.getResults<std::vector<std::string>>();
					if (!nameResults.empty()) {
						if (!database.query("EXECUTE selectModFilesStmt USING @paramModID;")) {
							std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
							break;
						}
						auto filesResults = database.getResults<std::vector<std::string>>();
						common::SendModManagementMessage::ModManagement mm;
						mm.modID = std::stoi(mod[0]);
						mm.majorVersion = uint8_t(std::stoi(mod[1]));
						mm.minorVersion = uint8_t(std::stoi(mod[2]));
						mm.patchVersion = uint8_t(std::stoi(mod[3]));
						mm.enabled = mod[4] == "1";
						mm.gothicVersion = common::GothicVersion(std::stoi(mod[5]));
						mm.type = common::ModType(std::stoi(mod[6]));
						mm.releaseDate = uint32_t(std::stoi(mod[7]));
						mm.name = nameResults[0][0];
						for (auto file : filesResults) {
							common::ModFile mf;
							mf.deleted = false;
							mf.filename = file[0];
							mf.hash = file[1];
							mf.language = file[2];
							mm.files.push_back(mf);
						}
						if (!database.query("EXECUTE selectEarlyAccessorsStmt USING @paramModID;")) {
							std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
							break;
						}
						auto userResults = database.getResults<std::vector<std::string>>();
						mm.userList.reserve(userResults.size());
						for (auto user : userResults) {
							std::string username = getUsername(std::stoi(user[0]));
							if (!username.empty()) {
								mm.userList.push_back(username);
							}
						}
						std::sort(mm.userList.begin(), mm.userList.end()); // names shall arrive sorted

						if (!database.query("EXECUTE selectDevDurationStmt USING @paramModID;")) {
							std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
							break;
						}
						auto durationResults = database.getResults<std::vector<std::string>>();
						if (durationResults.empty()) {
							mm.duration = 0;
						} else {
							mm.duration = std::stoi(durationResults[0][0]);
						}

						if (!database.query("EXECUTE selectScoreNamesStmt USING @paramModID;")) {
							std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
							break;
						}
						auto scoreNamesResults = database.getResults<std::vector<std::string>>();
						int id = -1;
						common::SendModManagementMessage::ModManagement::Score score;
						for (auto scoreName : scoreNamesResults) {
							const int currentID = std::stoi(scoreName[0]);
							if (currentID != id) {
								if (!score.names.empty()) {
									mm.scores.push_back(score);
								}
								score.names.clear();
								id = currentID;
							}
							common::TranslatedText tt;
							tt.language = scoreName[2];
							tt.text = scoreName[1];
							score.names.push_back(tt);
						}
						if (!score.names.empty()) {
							mm.scores.push_back(score);
						}

						if (!database.query("EXECUTE selectAchievementNamesStmt USING @paramModID;")) {
							std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
							break;
						}
						auto achievementNamesResults = database.getResults<std::vector<std::string>>();
						id = -1;
						common::SendModManagementMessage::ModManagement::Achievement achievement;
						common::SendModManagementMessage::ModManagement::Statistics::AchievementStatistic achievementStatistic;
						std::vector<common::SendModManagementMessage::ModManagement::Statistics::AchievementStatistic> achievementStatistics;
						for (auto achievementName : achievementNamesResults) {
							const int currentID = std::stoi(achievementName[0]);
							if (currentID != id) {
								if (!achievement.names.empty()) {
									mm.achievements.push_back(achievement);
								}
								achievement.names.clear();
								achievement.descriptions.clear();
								id = currentID;
							}
							common::TranslatedText tt;
							tt.language = achievementName[2];
							tt.text = achievementName[1];
							achievement.names.push_back(tt);

							if (!database.query("SET @paramIdentifier=" + std::to_string(id) + ";")) {
								std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
								break;
							}
							if (!database.query("SET @paramLanguage='" + tt.language + "';")) {
								std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
								break;
							}
							if (!database.query("EXECUTE selectAchievementDescriptionStmt USING @paramModID, @paramIdentifier, @paramLanguage;")) {
								std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
								break;
							}
							auto achievementDescriptionResults = database.getResults<std::vector<std::string>>();
							if (!achievementDescriptionResults.empty()) {
								tt.text = achievementDescriptionResults[0][0];
								achievement.descriptions.push_back(tt);
							}

							if (!database.query("EXECUTE selectAchievementProgressStmt USING @paramModID, @paramIdentifier;")) {
								std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
								break;
							}
							auto achievementProgressResults = database.getResults<std::vector<std::string>>();
							if (!achievementProgressResults.empty()) {
								achievement.maxProgress = std::stoi(achievementProgressResults[0][0]);
							} else {
								achievement.maxProgress = 0;
							}

							if (!database.query("EXECUTE selectAchievementHiddenStmt USING @paramModID, @paramIdentifier;")) {
								std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
								break;
							}
							auto achievementHiddenResults = database.getResults<std::vector<std::string>>();
							achievement.hidden = !achievementHiddenResults.empty();

							if (!database.query("EXECUTE selectAchievementIconsStmt USING @paramModID, @paramIdentifier;")) {
								std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
								break;
							}
							auto achievementIconsResults = database.getResults<std::vector<std::string>>();
							if (!achievementIconsResults.empty()) {
								achievement.lockedImageName = achievementIconsResults[0][0];
								achievement.lockedImageHash = achievementIconsResults[0][1];
								achievement.unlockedImageName = achievementIconsResults[0][2];
								achievement.unlockedImageHash = achievementIconsResults[0][3];
							} else {
								achievement.lockedImageName = "";
								achievement.lockedImageHash = "";
								achievement.unlockedImageName = "";
								achievement.unlockedImageHash = "";
							}

							if (tt.language == msg->language) {
								if (!database.query("EXECUTE selectAchievementtimeStmt USING @paramModID, @paramIdentifier;")) {
									std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
									break;
								}
								auto tmpResults = database.getResults<std::vector<std::string>>();
								uint32_t min = UINT32_MAX;
								uint32_t max = 0;
								uint64_t count = 0;
								for (size_t i = 0; i < tmpResults.size(); i++) {
									std::vector<std::string> tmpVec = tmpResults[i];
									const uint32_t current = uint32_t(std::stoi(tmpVec[0]));
									if (current < min) {
										min = current;
									}
									if (current > max) {
										max = current;
									}
									count += current;
									if (i == tmpResults.size() / 2) {
										achievementStatistic.medianTime = current;
									}
								}
								achievementStatistic.name = achievement.names.back().text;
								achievementStatistic.minTime = min == UINT32_MAX ? 0 : min;
								achievementStatistic.maxTime = max;
								if (tmpResults.empty()) {
									achievementStatistic.medianTime = 0;
									achievementStatistic.avgTime = 0;
								} else {
									achievementStatistic.avgTime = uint32_t(count / tmpResults.size());
								}
								achievementStatistics.push_back(achievementStatistic);
							}
						}
						if (!achievement.names.empty()) {
							mm.achievements.push_back(achievement);
						}

						// statistics
						common::SendModManagementMessage::ModManagement::Statistics statistics;
						if (!database.query("EXECUTE selectOverallDownloadsStmt USING @paramModID;")) {
							std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
							break;
						}
						auto tmpResults = database.getResults<std::vector<std::string>>();
						if (tmpResults.empty()) {
							statistics.overallDownloads = 0;
						} else {
							statistics.overallDownloads = std::stoi(tmpResults[0][0]);
						}
						if (!database.query("EXECUTE selectDownloadsPerVersionStmt USING @paramModID;")) {
							std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
							break;
						}
						tmpResults = database.getResults<std::vector<std::string>>();
						for (auto vec2 : tmpResults) {
							statistics.downloadsPerVersion.insert(std::make_pair(vec2[0], uint32_t(std::stoi(vec2[1]))));
						}
						if (!database.query("EXECUTE selectPlaytimeStmt USING @paramModID;")) {
							std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
							break;
						}
						tmpResults = database.getResults<std::vector<std::string>>();
						uint32_t min = UINT32_MAX;
						uint32_t max = 0;
						uint64_t count = 0;
						statistics.medianPlaytime = 0;
						for (size_t i = 0; i < tmpResults.size(); i++) {
							std::vector<std::string> vec2 = tmpResults[i];
							const uint32_t current = uint32_t(std::stoi(vec2[0]));
							if (current < min) {
								min = current;
							}
							if (current > max) {
								max = current;
							}
							count += current;
							if (i == tmpResults.size() / 2) {
								statistics.medianPlaytime = current;
							}
						}
						statistics.minPlaytime = min == UINT32_MAX ? 0 : min;
						statistics.maxPlaytime = max;
						if (tmpResults.empty()) {
							statistics.medianPlaytime = 0;
							statistics.avgPlaytime = 0;
						} else {
							statistics.avgPlaytime = uint32_t(count / tmpResults.size());
						}
						statistics.overallPlayerCount = tmpResults.size();
						if (!database.query("EXECUTE selectSessiontimeStmt USING @paramModID;")) {
							std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
							break;
						}
						tmpResults = database.getResults<std::vector<std::string>>();
						min = UINT32_MAX;
						max = 0;
						count = 0;
						statistics.medianSessiontime = 0;
						for (size_t i = 0; i < tmpResults.size(); i++) {
							std::vector<std::string> vec2 = tmpResults[i];
							const uint32_t current = uint32_t(std::stoi(vec2[0]));
							if (current < min) {
								min = current;
							}
							if (current > max) {
								max = current;
							}
							count += current;
							if (i == tmpResults.size() / 2) {
								statistics.medianSessiontime = current;
							}
						}
						statistics.minSessiontime = min == UINT32_MAX ? 0 : min;
						statistics.maxSessiontime = max;
						if (tmpResults.empty()) {
							statistics.medianSessiontime = 0;
							statistics.avgSessiontime = 0;
						} else {
							statistics.avgSessiontime = uint32_t(count / tmpResults.size());
						}
						statistics.achievementStatistics = achievementStatistics;

						const int timestamp = std::chrono::duration_cast<std::chrono::hours>(std::chrono::system_clock::now() - std::chrono::system_clock::time_point()).count();
						if (!database.query("SET @paramTimestamp=" + std::to_string(timestamp - 24) + ";")) {
							std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
							break;
						}
						if (!database.query("EXECUTE selectIntervalPlayersStmt USING @paramModID, @paramTimestamp;")) {
							std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
							break;
						}
						tmpResults = database.getResults<std::vector<std::string>>();
						statistics.last24HoursPlayerCount = uint32_t(std::stoi(tmpResults[0][0]));

						if (!database.query("SET @paramTimestamp=" + std::to_string(timestamp - 7 * 24) + ";")) {
							std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
							break;
						}
						if (!database.query("EXECUTE selectIntervalPlayersStmt USING @paramModID, @paramTimestamp;")) {
							std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
							break;
						}
						tmpResults = database.getResults<std::vector<std::string>>();
						statistics.last7DaysPlayerCount = uint32_t(std::stoi(tmpResults[0][0]));

						mm.statistics = statistics;

						if (!database.query("EXECUTE selectChapterStatsStmt USING @paramModID;")) {
							std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
							break;
						}
						tmpResults = database.getResults<std::vector<std::string>>();
						for (auto chapterStatsVec : tmpResults) {
							int32_t identifier = std::stoi(chapterStatsVec[0]);
							int32_t guild = std::stoi(chapterStatsVec[1]);
							const std::string statName = chapterStatsVec[2];
							const int32_t statValue = std::stoi(chapterStatsVec[3]);

							common::SendModManagementMessage::ModManagement::CustomStatistic cs;
							cs.name = statName;
							cs.value = statValue;

							mm.customStatistics[std::make_pair(identifier, guild)].push_back(cs);
						}

						smmm.modList.push_back(mm);
					}
				}
			}
			smmm.userList = getUserList();
		} while (false);
		const std::string serialized = smmm.SerializePrivate();
		sock->writePacket(serialized);
	}

	void Server::handleUpdateModVersion(clockUtils::sockets::TcpSocket *, common::UpdateModVersionMessage * msg) const {
		do {
			MariaDBWrapper database;
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("PREPARE updateStmt FROM \"UPDATE mods SET MajorVersion = ?, MinorVersion = ?, PatchVersion = ? WHERE ModID = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramModID=" + std::to_string(msg->modID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramMajorVersion=" + std::to_string(int(msg->majorVersion)) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramMinorVersion=" + std::to_string(int(msg->minorVersion)) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramPatchVersion=" + std::to_string(int(msg->patchVersion)) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("EXECUTE updateStmt USING @paramMajorVersion, @paramMinorVersion, @paramPatchVersion, @paramModID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
		} while (false);
	}

	void Server::handleUpdateEarlyAccessState(clockUtils::sockets::TcpSocket *, common::UpdateEarlyAccessStateMessage * msg) const {
		do {
			const int userID = getUserID(msg->username, msg->password);
			if (userID == -1) {
				break;
			}
			MariaDBWrapper database;
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("PREPARE insertStmt FROM \"INSERT INTO earlyUnlocks (ModID, UserID) VALUES (?, ?)\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE deleteStmt FROM \"DELETE FROM earlyUnlocks WHERE ModID = ? AND UserID = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramModID=" + std::to_string(msg->modID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (msg->enabled) {
				if (!database.query("EXECUTE insertStmt USING @paramModID, @paramUserID;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
			} else {
				if (!database.query("EXECUTE deleteStmt USING @paramModID, @paramUserID;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
			}
		} while (false);
	}

	void Server::handleRequestModsForEditor(clockUtils::sockets::TcpSocket * sock, common::RequestModsForEditorMessage * msg) const {
		common::SendModsForEditorMessage smfem;
		do {
			const int userID = getUserID(msg->username, msg->password);
			if (userID == -1) {
				break;
			}
			MariaDBWrapper database;
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("PREPARE selectTeamsStmt FROM \"SELECT TeamID FROM teammembers WHERE UserID = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectModStmt FROM \"SELECT ModID FROM mods WHERE TeamID = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectModNameStmt FROM \"SELECT CAST(Name AS BINARY) FROM modnames WHERE ModID = ? AND Language = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectLockedAchievementImagesStmt FROM \"SELECT LockedIcon, LockedHash FROM modAchievementIcons WHERE ModID = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectUnlockedAchievementImagesStmt FROM \"SELECT UnlockedIcon, UnlockedHash FROM modAchievementIcons WHERE ModID = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramLanguage='" + msg->language + "';")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("EXECUTE selectTeamsStmt USING @paramUserID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			auto lastResults = database.getResults<std::vector<std::string>>();
			for (auto vec : lastResults) {
				if (!database.query("SET @paramTeamID=" + vec[0] + ";")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("EXECUTE selectModStmt USING @paramTeamID;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
				auto results = database.getResults<std::vector<std::string>>();
				for (auto mod : results) {
					if (!database.query("SET @paramModID=" + mod[0] + ";")) {
						std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
						break;
					}
					if (!database.query("EXECUTE selectModNameStmt USING @paramModID, @paramLanguage;")) {
						std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
						break;
					}
					auto nameResults = database.getResults<std::vector<std::string>>();
					if (!nameResults.empty()) {
						common::SendModsForEditorMessage::ModForEditor mfe;
						mfe.modID = std::stoi(mod[0]);
						mfe.name = nameResults[0][0];
						if (!database.query("EXECUTE selectLockedAchievementImagesStmt USING @paramModID;")) {
							std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
							break;
						}
						auto imageResults = database.getResults<std::vector<std::string>>();
						std::set<std::string> images;
						for (auto img : imageResults) {
							if (images.find(img[0]) == images.end()) {
								mfe.images.emplace_back(img[0], img[1]);
								images.insert(img[0]);
							}
						}
						if (!database.query("EXECUTE selectUnlockedAchievementImagesStmt USING @paramModID;")) {
							std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
							break;
						}
						imageResults = database.getResults<std::vector<std::string>>();
						for (auto img : imageResults) {
							if (images.find(img[0]) == images.end()) {
								mfe.images.emplace_back(img[0], img[1]);
								images.insert(img[0]);
							}
						}
						smfem.modList.push_back(mfe);
					}
				}
			}
		} while (false);
		const std::string serialized = smfem.SerializePrivate();
		sock->writePacket(serialized);
	}

	void Server::handleUpdateOfflineData(clockUtils::sockets::TcpSocket *, common::UpdateOfflineDataMessage * msg) const {
		do {
			const int userID = getUserID(msg->username, msg->password);
			if (userID == -1) {
				break;
			}
			MariaDBWrapper database;
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("PREPARE insertAchievementStmt FROM \"INSERT IGNORE INTO modAchievements (ModID, Identifier, UserID) VALUES (?, ?, ?)\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE updateAchievementProgress FROM \"INSERT INTO modAchievementProgress (ModID, Identifier, UserID, Current) VALUES (?, ?, ?, ?) ON DUPLICATE KEY UPDATE Current = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE updateScoresStmt FROM \"INSERT INTO modScores (ModID, Identifier, UserID, Score) VALUES (?, ?, ?, ?) ON DUPLICATE KEY UPDATE Score = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE updateOverallSaveStmt FROM \"INSERT INTO overallSaveData (ModID, UserID, Entry, Value) VALUES (?, ?, ?, ?) ON DUPLICATE KEY UPDATE Value = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE updatePlayTimeStmt FROM \"INSERT INTO playtimes (ModID, UserID, Duration) VALUES (?, ?, ?) ON DUPLICATE KEY UPDATE Duration = Duration + ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			for (auto a : msg->achievements) {
				if (!database.query("SET @paramModID=" + std::to_string(a.modID) + ";")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("SET @paramIdentifier=" + std::to_string(a.identifier) + ";")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("EXECUTE insertAchievementStmt USING @paramModID, @paramIdentifier, @paramUserID;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
				if (a.current > 0) {
					if (!database.query("SET @paramCurrent=" + std::to_string(a.current) + ";")) {
						std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
						break;
					}
					if (!database.query("EXECUTE updateAchievementProgress USING @paramModID, @paramIdentifier, @paramUserID, @paramCurrent, @paramCurrent;")) {
						std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
						break;
					}
				}
			}
			for (auto s : msg->scores) {
				if (!database.query("SET @paramModID=" + std::to_string(s.modID) + ";")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("SET @paramIdentifier=" + std::to_string(s.identifier) + ";")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("SET @paramScore=" + std::to_string(s.score) + ";")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("EXECUTE updateScoresStmt USING @paramModID, @paramIdentifier, @paramUserID, @paramScore, @paramScore;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
			}
			for (auto o : msg->overallSaves) {
				if (!database.query("SET @paramModID=" + std::to_string(o.modID) + ";")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("SET @paramEntry='" + o.entry + "';")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("SET @paramValue='" + o.value + "';")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("EXECUTE updateOverallSaveStmt USING @paramModID, @paramUserID, @paramEntry, @paramValue, @paramValue;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
			}
			for (const auto p : msg->playTimes) {
				if (!database.query("SET @paramModID=" + std::to_string(p.first) + ";")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("SET @paramDuration=" + std::to_string(p.second) + ";")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("EXECUTE updatePlayTimeStmt USING @paramModID, @paramUserID, @paramDuration, @paramDuration;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
			}
		} while (false);
	}

	void Server::handleRequestOfflineData(clockUtils::sockets::TcpSocket * sock, common::RequestOfflineDataMessage * msg) const {
		common::SendOfflineDataMessage sodm;
		do {
			const int userID = getUserID(msg->username, msg->password);
			if (userID == -1) {
				break;
			}
			MariaDBWrapper database;
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("PREPARE selectAchievementsStmt FROM \"SELECT ModID, Identifier FROM modAchievementList\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectOwnAchievementsStmt FROM \"SELECT ModID FROM modAchievements WHERE ModID = ? AND Identifier = ? AND UserID = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectModAchievementProgressStmt FROM \"SELECT Current FROM modAchievementProgress WHERE ModID = ? AND Identifier = ? AND UserID = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectModAchievementProgressMaxStmt FROM \"SELECT Max FROM modAchievementProgressMax WHERE ModID = ? AND Identifier = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectScoresStmt FROM \"SELECT ModID, Identifier, UserID, Score FROM modScores\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectOverallSavesStmt FROM \"SELECT ModID, Entry, Value FROM overallSaveData WHERE UserID = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("EXECUTE selectAchievementsStmt;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			auto lastResults = database.getResults<std::vector<std::string>>();
			for (auto vec : lastResults) {
				common::SendOfflineDataMessage::AchievementData ad;
				ad.modID = std::stoi(vec[0]);
				ad.identifier = std::stoi(vec[1]);
				if (!database.query("SET @paramModID=" + vec[0] + ";")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("SET @paramIdentifier=" + vec[1] + ";")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("EXECUTE selectOwnAchievementsStmt USING @paramModID, @paramIdentifier, @paramUserID;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
				auto results = database.getResults<std::vector<std::string>>();
				ad.unlocked = !results.empty();
				if (!database.query("EXECUTE selectModAchievementProgressStmt USING @paramModID, @paramIdentifier, @paramUserID;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
				results = database.getResults<std::vector<std::string>>();
				if (results.empty()) {
					ad.current = 0;
				} else {
					ad.current = std::stoi(results[0][0]);
				}
				if (!database.query("EXECUTE selectModAchievementProgressMaxStmt USING @paramModID, @paramIdentifier;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
				results = database.getResults<std::vector<std::string>>();
				if (results.empty()) {
					ad.max = 0;
				} else {
					ad.max = std::stoi(results[0][0]);
				}
				ad.username = msg->username;
				sodm.achievements.push_back(ad);
			}
			if (!database.query("EXECUTE selectScoresStmt;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			lastResults = database.getResults<std::vector<std::string>>();
			for (auto vec : lastResults) {
				common::SendOfflineDataMessage::ScoreData sd;
				sd.modID = std::stoi(vec[0]);
				sd.identifier = std::stoi(vec[1]);
				sd.username = getUsername(std::stoi(vec[2]));
				sd.score = std::stoi(vec[3]);
				sodm.scores.push_back(sd);
			}
			if (!database.query("EXECUTE selectOverallSavesStmt USING @paramUserID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			lastResults = database.getResults<std::vector<std::string>>();
			for (auto vec : lastResults) {
				common::SendOfflineDataMessage::OverallSaveData od;
				od.modID = std::stoi(vec[0]);
				od.entry = vec[1];
				od.value = vec[2];
				od.username = msg->username;
				sodm.overallSaves.push_back(od);
			}
		} while (false);
		const std::string serialized = sodm.SerializePrivate();
		sock->writePacket(serialized);
	}

	void Server::handleUpdateStartTime(clockUtils::sockets::TcpSocket *, common::UpdateStartTimeMessage * msg) const {
		do {
			MariaDBWrapper database;
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("PREPARE insertStmt FROM \"INSERT INTO startTimes (DayOfWeek, Hour, Counter) VALUES (?, ?, 1) ON DUPLICATE KEY UPDATE Counter = Counter + 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramDayOfWeek=" + std::to_string(msg->dayOfTheWeek) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramHour=" + std::to_string(msg->hour) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("EXECUTE insertStmt USING @paramDayOfWeek, @paramHour;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
		} while (false);
	}

	void Server::handleUpdatePlayingTime(clockUtils::sockets::TcpSocket *, common::UpdatePlayingTimeMessage * msg) const {
		do {
			MariaDBWrapper database;
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("PREPARE insertStmt FROM \"INSERT INTO playingTimes (DayOfWeek, Hour, Counter) VALUES (?, ?, 1) ON DUPLICATE KEY UPDATE Counter = Counter + 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramDayOfWeek=" + std::to_string(msg->dayOfTheWeek) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramHour=" + std::to_string(msg->hour) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("EXECUTE insertStmt USING @paramDayOfWeek, @paramHour;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
		} while (false);
	}

	void Server::handleRequestAllFriends(clockUtils::sockets::TcpSocket * sock, common::RequestAllFriendsMessage * msg) const {
		common::SendAllFriendsMessage safm;
		do {
			const int userID = getUserID(msg->username, msg->password);
			if (userID == -1) {
				break;
			}
			std::vector<std::string> allUsers;
			{
				MariaDBWrapper database;
				if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, ACCOUNTSDATABASE, 0)) {
					std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
				if (!database.query("PREPARE selectStmt FROM \"SELECT Username FROM accounts ORDER BY Username ASC\";")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("EXECUTE selectStmt;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
				auto lastResults = database.getResults<std::vector<std::string>>();
				for (auto vec : lastResults) {
					allUsers.push_back(vec[0]);
				}
			}
			MariaDBWrapper database;
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("PREPARE selectOwnFriendsStmt FROM \"SELECT FriendID FROM friends WHERE UserID = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE checkIfFriendStmt FROM \"SELECT UserID FROM friends WHERE UserID = ? AND FriendID = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectRequestsStmt FROM \"SELECT UserID FROM friends WHERE FriendID = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("EXECUTE selectOwnFriendsStmt USING @paramUserID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			auto lastResults = database.getResults<std::vector<std::string>>();
			for (auto vec : lastResults) {
				if (!database.query("SET @paramFriendID=" + vec[0] + ";")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("EXECUTE checkIfFriendStmt USING @paramFriendID, @paramUserID;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
				std::string friendName = getUsername(std::stoi(vec[0]));
				auto results = database.getResults<std::vector<std::string>>();
				if (results.empty()) {
					uint32_t currentXP, nextXP;
					safm.pendingFriends.emplace_back(friendName, getLevel(std::stoi(vec[0]), currentXP, nextXP));
				} else {
					uint32_t currentXP, nextXP;
					safm.friends.emplace_back(friendName, getLevel(std::stoi(vec[0]), currentXP, nextXP));
				}
				allUsers.erase(std::remove_if(allUsers.begin(), allUsers.end(), [friendName](const std::string & o) { return o == friendName; }), allUsers.end());
			}
			if (!database.query("EXECUTE selectRequestsStmt USING @paramUserID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			lastResults = database.getResults<std::vector<std::string>>();
			for (auto vec : lastResults) {
				const std::string friendName = getUsername(std::stoi(vec[0]));
				if (std::find_if(safm.friends.begin(), safm.friends.end(), [friendName](const common::Friend & o) { return o.name == friendName; }) == safm.friends.end()) {
					uint32_t currentXP, nextXP;
					safm.friendRequests.emplace_back(friendName, getLevel(std::stoi(vec[0]), currentXP, nextXP));
					allUsers.erase(std::remove_if(allUsers.begin(), allUsers.end(), [friendName](const std::string & o) { return o == friendName; }), allUsers.end());
				}
			}

			safm.nonFriends = allUsers;
		} while (false);
		std::sort(safm.friends.begin(), safm.friends.end(), [](const common::Friend & a, const common::Friend & b) {
			return a.name < b.name;
		});
		const std::string serialized = safm.SerializePrivate();
		sock->writePacket(serialized);
	}

	void Server::handleSendFriendRequest(clockUtils::sockets::TcpSocket *, common::SendFriendRequestMessage * msg) const {
		do {
			const int userID = getUserID(msg->username, msg->password);
			const int friendID = getUserID(msg->friendname);
			if (userID == -1 || friendID == -1 || userID == friendID) {
				break;
			}
			MariaDBWrapper database;
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("PREPARE insertStmt FROM \"INSERT INTO friends (UserID, FriendID) VALUES (?, ?)\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramFriendID=" + std::to_string(friendID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("EXECUTE insertStmt USING @paramUserID, @paramFriendID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
		} while (false);
	}

	void Server::handleAcceptFriendRequest(clockUtils::sockets::TcpSocket *, common::AcceptFriendRequestMessage * msg) const {
		do {
			const int userID = getUserID(msg->username, msg->password);
			const int friendID = getUserID(msg->friendname);
			if (userID == -1 || friendID == -1 || userID == friendID) {
				break;
			}
			MariaDBWrapper database;
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("PREPARE insertStmt FROM \"INSERT INTO friends (UserID, FriendID) VALUES (?, ?)\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramFriendID=" + std::to_string(friendID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("EXECUTE insertStmt USING @paramUserID, @paramFriendID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
		} while (false);
	}

	void Server::handleDeclineFriendRequest(clockUtils::sockets::TcpSocket *, common::DeclineFriendRequestMessage * msg) const {
		do {
			const int userID = getUserID(msg->username, msg->password);
			const int friendID = getUserID(msg->friendname);
			if (userID == -1 || friendID == -1 || userID == friendID) {
				break;
			}
			MariaDBWrapper database;
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("PREPARE deleteStmt FROM \"DELETE FROM friends WHERE UserID = ? AND FriendID = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramFriendID=" + std::to_string(friendID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("EXECUTE deleteStmt USING @paramFriendID, @paramUserID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
		} while (false);
	}

	void Server::handleRequestUserLevel(clockUtils::sockets::TcpSocket * sock, common::RequestUserLevelMessage * msg) const {
		common::SendUserLevelMessage sulm;
		do {
			const int userID = getUserID(msg->username, msg->password);
			if (userID == -1) {
				break;
			}
			uint32_t currentXP, nextXP;
			sulm.level = getLevel(userID, currentXP, nextXP);
			sulm.currentXP = currentXP;
			sulm.nextXP = nextXP;
		} while (false);
		const std::string serialized = sulm.SerializePrivate();
		sock->writePacket(serialized);
	}

	void Server::handleUpdateGeneralModConfiguration(clockUtils::sockets::TcpSocket *, common::UpdateGeneralModConfigurationMessage * msg) const {
		do {
			MariaDBWrapper database;
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("PREPARE updateStmt FROM \"UPDATE mods SET Enabled = ?, Gothic = ?, Type = ?, ReleaseDate = ? WHERE ModID = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE updateDevDurationStmt FROM \"INSERT INTO devtimes (ModID, Duration) VALUES (?, ?) ON DUPLICATE KEY UPDATE Duration = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramModID=" + std::to_string(msg->modID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramEnabled=" + std::to_string(msg->enabled) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramGothicVersion=" + std::to_string(int(msg->gothicVersion)) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramModType=" + std::to_string(int(msg->modType)) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramReleaseDate=" + std::to_string(msg->releaseDate) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramDuration=" + std::to_string(msg->duration) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("EXECUTE updateStmt USING @paramEnabled, @paramGothicVersion, @paramModType, @paramReleaseDate, @paramModID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("EXECUTE updateDevDurationStmt USING @paramModID, @paramDuration, @paramDuration;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
		} while (false);
	}

	void Server::handleUpdateScores(clockUtils::sockets::TcpSocket *, common::UpdateScoresMessage * msg) const {
		do {
			MariaDBWrapper database;
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("PREPARE insertScoreStmt FROM \"INSERT IGNORE INTO modScoreList (ModID, Identifier) VALUES (?, ?)\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE updateScoreNameStmt FROM \"INSERT INTO modScoreNames (ModID, Identifier, Language, Name) VALUES (?, ?, CONVERT(? USING BINARY), CONVERT(? USING BINARY)) ON DUPLICATE KEY UPDATE Name = CONVERT(? USING BINARY)\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramModID=" + std::to_string(msg->modID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			for (size_t i = 0; i < msg->scores.size(); i++) {
				if (!database.query("SET @paramIdentifier=" + std::to_string(i) + ";")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("EXECUTE insertScoreStmt USING @paramModID, @paramIdentifier;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
				for (const auto & tt : msg->scores[i].names) {
					if (!database.query("SET @paramLanguage='" + tt.language + "';")) {
						std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
						break;
					}
					if (!database.query("SET @paramName='" + tt.text + "';")) {
						std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
						break;
					}
					if (!database.query("EXECUTE updateScoreNameStmt USING @paramModID, @paramIdentifier, @paramLanguage, @paramName, @paramName;")) {
						std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
						break;
					}
				}
			}
		} while (false);
	}

	void Server::handleUpdateAchievements(clockUtils::sockets::TcpSocket *, common::UpdateAchievementsMessage * msg) const {
		do {
			MariaDBWrapper database;
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("PREPARE insertAchievementStmt FROM \"INSERT IGNORE INTO modAchievementList (ModID, Identifier) VALUES (?, ?)\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE updateAchievementNameStmt FROM \"INSERT INTO modAchievementNames (ModID, Identifier, Language, Name) VALUES (?, ?, CONVERT(? USING BINARY), CONVERT(? USING BINARY)) ON DUPLICATE KEY UPDATE Name = CONVERT(? USING BINARY)\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE updateAchievementDescriptionStmt FROM \"INSERT INTO modAchievementDescriptions (ModID, Identifier, Language, Description) VALUES (?, ?, CONVERT(? USING BINARY), CONVERT(? USING BINARY)) ON DUPLICATE KEY UPDATE Description = CONVERT(? USING BINARY)\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE updateAchievementProgressStmt FROM \"INSERT INTO modAchievementProgressMax (ModID, Identifier, Max) VALUES (?, ?, ?) ON DUPLICATE KEY UPDATE Max = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE deleteAchievementProgressStmt FROM \"DELETE FROM modAchievementProgressMax WHERE ModID = ? AND Identifier = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE updateAchievementHiddenStmt FROM \"INSERT IGNORE INTO modAchievementHidden (ModID, Identifier) VALUES (?, ?)\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE deleteAchievementHiddenStmt FROM \"DELETE FROM modAchievementHidden WHERE ModID = ? AND Identifier = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE updateAchievementIconStmt FROM \"INSERT INTO modAchievementIcons (ModID, Identifier, LockedIcon, LockedHash, UnlockedIcon, UnlockedHash) VALUES (?, ?, ?, ?, ?, ?) ON DUPLICATE KEY UPDATE LockedIcon = ?, LockedHash = ?, UnlockedIcon = ?, UnlockedHash = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramModID=" + std::to_string(msg->modID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			for (size_t i = 0; i < msg->achievements.size(); i++) {
				if (!database.query("SET @paramIdentifier=" + std::to_string(i) + ";")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("EXECUTE insertAchievementStmt USING @paramModID, @paramIdentifier;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
				auto achievement = msg->achievements[i];
				for (const auto & tt : achievement.names) {
					if (!database.query("SET @paramLanguage='" + tt.language + "';")) {
						std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
						break;
					}
					if (!database.query("SET @paramName='" + tt.text + "';")) {
						std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
						break;
					}
					if (!database.query("EXECUTE updateAchievementNameStmt USING @paramModID, @paramIdentifier, @paramLanguage, @paramName, @paramName;")) {
						std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
						break;
					}
				}
				for (const auto & tt : achievement.descriptions) {
					if (!database.query("SET @paramLanguage='" + tt.language + "';")) {
						std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
						break;
					}
					if (!database.query("SET @paramDescription='" + tt.text + "';")) {
						std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
						break;
					}
					if (!database.query("EXECUTE updateAchievementDescriptionStmt USING @paramModID, @paramIdentifier, @paramLanguage, @paramDescription, @paramDescription;")) {
						std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
						break;
					}
				}
				if (achievement.maxProgress > 0) {
					if (!database.query("SET @paramProgress=" + std::to_string(achievement.maxProgress) + ";")) {
						std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
						break;
					}
					if (!database.query("EXECUTE updateAchievementProgressStmt USING @paramModID, @paramIdentifier, @paramProgress, @paramProgress;")) {
						std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
						break;
					}
				} else {
					if (!database.query("EXECUTE deleteAchievementProgressStmt USING @paramModID, @paramIdentifier;")) {
						std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
						break;
					}
				}
				if (achievement.hidden) {
					if (!database.query("EXECUTE updateAchievementHiddenStmt USING @paramModID, @paramIdentifier;")) {
						std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
						break;
					}
				} else {
					if (!database.query("EXECUTE deleteAchievementHiddenStmt USING @paramModID, @paramIdentifier;")) {
						std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
						break;
					}
				}
				boost::filesystem::create_directories("/var/www/vhosts/clockwork-origins.de/httpdocs/Gothic/downloads/mods/" + std::to_string(msg->modID) + "/achievements");
				if (achievement.lockedImageData.empty()) {
					std::ofstream out;
					out.open("/var/www/vhosts/clockwork-origins.de/httpdocs/Gothic/downloads/mods/" + std::to_string(msg->modID) + "/achievements/" + achievement.lockedImageName, std::ios::out | std::ios::binary);
					out.write(reinterpret_cast<char *>(&achievement.lockedImageData[0]), achievement.lockedImageData.size());
					out.close();
				}
				if (achievement.unlockedImageData.empty()) {
					std::ofstream out;
					out.open("/var/www/vhosts/clockwork-origins.de/httpdocs/Gothic/downloads/mods/" + std::to_string(msg->modID) + "/achievements/" + achievement.unlockedImageName, std::ios::out | std::ios::binary);
					out.write(reinterpret_cast<char *>(&achievement.unlockedImageData[0]), achievement.unlockedImageData.size());
					out.close();
				}
				if (!achievement.lockedImageData.empty() || !achievement.unlockedImageData.empty()) {
					if (!database.query("SET @paramLockedIcon='" + achievement.lockedImageName + "';")) {
						std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
						break;
					}
					if (!database.query("SET @paramLockedHash='" + achievement.lockedImageHash + "';")) {
						std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
						break;
					}
					if (!database.query("SET @paramUnlockedIcon='" + achievement.unlockedImageName + "';")) {
						std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
						break;
					}
					if (!database.query("SET @paramUnlockedHash='" + achievement.unlockedImageHash + "';")) {
						std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
						break;
					}
					if (!database.query("EXECUTE updateAchievementIconStmt USING @paramModID, @paramIdentifier, @paramLockedIcon, @paramLockedHash, @paramUnlockedIcon, @paramUnlockedHash, @paramLockedIcon, @paramLockedHash, @paramUnlockedIcon, @paramUnlockedHash;")) {
						std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
						break;
					}
				}
			}
		} while (false);
	}

	void Server::handleUpdateSucceeded(clockUtils::sockets::TcpSocket *, common::UpdateSucceededMessage * msg) const {
		do {
			MariaDBWrapper database;
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("PREPARE insertStmt FROM \"INSERT INTO downloadsPerVersion (ModID, Version, Counter) VALUES (?, CONVERT(? USING BINARY), 1) ON DUPLICATE KEY UPDATE Counter = Counter + 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectVersionStmt FROM \"SELECT MajorVersion, MinorVersion, PatchVersion FROM mods WHERE ModID = ? LIMIT 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramModID=" + std::to_string(msg->modID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("EXECUTE selectVersionStmt USING @paramModID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			auto results = database.getResults<std::vector<std::string>>();
			if (!results.empty()) {
				const std::string version = results[0][0] + "." + results[0][1] + "." + results[0][2];
				if (!database.query("SET @paramVersion='" + version + "';")) {
					std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
					break;
				}
				if (!database.query("EXECUTE insertStmt USING @paramModID, @paramVersion;")) {
					std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
					break;
				}
			}
		} while (false);
	}

	void Server::handleUpdateChapterStats(clockUtils::sockets::TcpSocket *, common::UpdateChapterStatsMessage * msg) const {
		do {
			MariaDBWrapper database;
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("PREPARE insertStmt FROM \"INSERT INTO chapterStats (ModID, Identifier, Guild, StatName, StatValue) VALUES (?, ?, ?, CONVERT(? USING BINARY), ?)\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramModID=" + std::to_string(msg->modID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramIdentifier=" + std::to_string(msg->identifier) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramGuild=" + std::to_string(msg->guild) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramStatName='" + msg->statName + "';")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramStatValue=" + std::to_string(msg->statValue) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("EXECUTE insertStmt USING @paramModID, @paramIdentifier, @paramGuild, @paramStatName, @paramStatValue;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
		} while (false);
	}

	void Server::handleUpdateImpression(clockUtils::sockets::TcpSocket *, common::UpdateImpressionMessage * msg) const {
		do {
			MariaDBWrapper database;
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("PREPARE insertStmt FROM \"INSERT INTO impressions (Year, Month, Count) VALUES (?, ?, 1) ON DUPLICATE KEY UPDATE Count = Count + 1\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramYear=" + std::to_string(msg->year) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramMonth=" + std::to_string(msg->month) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("EXECUTE insertStmt USING @paramYear, @paramMonth;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
		} while (false);
	}

	int Server::getUserID(const std::string & username, const std::string & password) const {
		MariaDBWrapper accountDatabase;
		if (!accountDatabase.connect("localhost", DATABASEUSER, DATABASEPASSWORD, ACCOUNTSDATABASE, 0)) {
			std::cout << "Couldn't connect to database" << std::endl;
			return -1;
		}

		if (!accountDatabase.query("PREPARE selectStmt FROM \"SELECT ID FROM accounts WHERE Username = ? AND Password = PASSWORD(?) LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return -1;
		}
		if (!accountDatabase.query("SET @paramUsername='" + username + "';")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return -1;
		}
		if (!accountDatabase.query("SET @paramPassword='" + password + "';")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return -1;
		}
		if (!accountDatabase.query("EXECUTE selectStmt USING @paramUsername, @paramPassword;")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
		}
		std::vector<std::vector<std::string>> results = accountDatabase.getResults<std::vector<std::string>>();
		accountDatabase.close();
		if (results.empty()) {
			return -1;
		} else {
			return std::stoi(results[0][0]);
		}
	}

	int Server::getUserID(const std::string & username) const {
		MariaDBWrapper accountDatabase;
		if (!accountDatabase.connect("localhost", DATABASEUSER, DATABASEPASSWORD, ACCOUNTSDATABASE, 0)) {
			std::cout << "Couldn't connect to database" << std::endl;
			return -1;
		}

		if (!accountDatabase.query("PREPARE selectStmt FROM \"SELECT ID FROM accounts WHERE Username = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return -1;
		}
		if (!accountDatabase.query("SET @paramUsername='" + username + "';")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return -1;
		}
		if (!accountDatabase.query("EXECUTE selectStmt USING @paramUsername;")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
		}
		std::vector<std::vector<std::string>> results = accountDatabase.getResults<std::vector<std::string>>();
		accountDatabase.close();
		if (results.empty()) {
			return -1;
		} else {
			return std::stoi(results[0][0]);
		}
	}

	std::string Server::getUsername(const int id) const {
		MariaDBWrapper accountDatabase;
		if (!accountDatabase.connect("localhost", DATABASEUSER, DATABASEPASSWORD, ACCOUNTSDATABASE, 0)) {
			return "";
		}

		if (!accountDatabase.query("PREPARE selectStmt FROM \"SELECT Username FROM accounts WHERE ID = ? LIMIT 1\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return "";
		}
		if (!accountDatabase.query("SET @paramID=" + std::to_string(id) + ";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return "";
		}
		if (!accountDatabase.query("EXECUTE selectStmt USING @paramID;")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
		}
		std::vector<std::vector<std::string>> results = accountDatabase.getResults<std::vector<std::string>>();
		if (results.empty()) {
			return "";
		} else {
			return results[0][0];
		}
	}

	std::vector<std::string> Server::getUserList() {
		MariaDBWrapper accountDatabase;
		if (!accountDatabase.connect("localhost", DATABASEUSER, DATABASEPASSWORD, ACCOUNTSDATABASE, 0)) {
			return {};
		}

		if (!accountDatabase.query("PREPARE selectStmt FROM \"SELECT Username FROM accounts ORDER BY Username ASC\";")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
			return {};
		}
		if (!accountDatabase.query("EXECUTE selectStmt;")) {
			std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
		}
		std::vector<std::vector<std::string>> results = accountDatabase.getResults<std::vector<std::string>>();
		std::vector<std::string> users;
		users.reserve(results.size());
		for (auto vec : results) {
			users.push_back(vec[0]);
		}
		return users;
	}

	void Server::sendMail(const std::string & subject, const std::string & body) {
		std::thread([subject, body]() {
			Smtp s("127.0.0.1");
			bool b = s.sendMail("contact@clockwork-origins.de", "bonne@clockwork-origins.de", subject, body);
			static_cast<void>(b);
		}).detach();
	}

	uint32_t Server::getLevel(const int userID, uint32_t & currentXP, uint32_t & nextXP) const {
		int level = 0;
		currentXP = 0;

		do {
			MariaDBWrapper database;
			if (!database.connect("localhost", DATABASEUSER, DATABASEPASSWORD, SPINEDATABASE, 0)) {
				std::cout << "Couldn't connect to database: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			if (!database.query("PREPARE selectAchievementsStmt FROM \"SELECT COUNT(*) FROM modAchievements WHERE UserID = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectScoresStmt FROM \"SELECT Count(*) FROM modScores WHERE UserID = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectRatingsStmt FROM \"SELECT Count(*) FROM ratings WHERE UserID = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("PREPARE selectCompatibilitiesStmt FROM \"SELECT Count(*) FROM compatibilityList WHERE UserID = ?\";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("SET @paramUserID=" + std::to_string(userID) + ";")) {
				std::cout << "Query couldn't be started: " << __LINE__ << std::endl;
				break;
			}
			if (!database.query("EXECUTE selectAchievementsStmt USING @paramUserID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			auto lastResults = database.getResults<std::vector<std::string>>();
			if (!lastResults.empty()) {
				currentXP += std::stoi(lastResults[0][0]) * 50; // 50 EP per achievement
			}
			if (!database.query("EXECUTE selectScoresStmt USING @paramUserID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			lastResults = database.getResults<std::vector<std::string>>();
			if (!lastResults.empty()) {
				currentXP += std::stoi(lastResults[0][0]) * 100; // 100 EP per score
			}
			if (!database.query("EXECUTE selectRatingsStmt USING @paramUserID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			lastResults = database.getResults<std::vector<std::string>>();
			if (!lastResults.empty()) {
				currentXP += std::stoi(lastResults[0][0]) * 250; // 250 EP per rating
			}
			if (!database.query("EXECUTE selectCompatibilitiesStmt USING @paramUserID;")) {
				std::cout << "Query couldn't be started: " << __LINE__ << /*" " << database.getLastError() <<*/ std::endl;
				break;
			}
			lastResults = database.getResults<std::vector<std::string>>();
			if (!lastResults.empty()) {
				currentXP += std::stoi(lastResults[0][0]) * 10; // 10 EP per compatibility list entry
			}
		} while (false);

		nextXP = 500;
		while (currentXP >= nextXP) {
			level++;
			nextXP += (level + 1) * 500;
		}

		return level;
	}

} /* namespace spine */
