/*
 Flexisip, a flexible SIP proxy server with media capabilities.
 Copyright (C) 2012  Belledonne Communications SARL.

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as
 published by the Free Software Foundation, either version 3 of the
 License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PUSH_NOTIFICATION_CLIENT_H
#define PUSH_NOTIFICATION_CLIENT_H

#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include "pushnotificationservice.h"

class PushNotificationClient {
public:
	PushNotificationClient(const std::string &name, PushNotificationService *service);

	void setData(const std::vector<char> &data);
	const std::vector<char> &getData();

	void start(const std::string &host, const std::string &port);

	void handle_resolve(const boost::system::error_code& error, boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
	void handle_connect(const boost::system::error_code& error, boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
	void handle_handshake(const boost::system::error_code& error);
	void handle_write(const boost::system::error_code& error, size_t bytes_transferred);
	void handle_read(const boost::system::error_code& error, size_t bytes_transferred);
	bool isReady() const;

private:
	void connect(const std::string &host, const std::string &port);
	void send();
	void onEnd();
	void onError();

private:
	PushNotificationService *mService;
	boost::asio::ip::tcp::resolver mResolver;
	boost::asio::ssl::stream<boost::asio::ip::tcp::socket> mSocket;
	bool mReady;
	std::vector<char> mData;
	boost::asio::streambuf mResponse;
	std::string mName;
};

#endif //PUSH_NOTIFICATION_CLIENT_H