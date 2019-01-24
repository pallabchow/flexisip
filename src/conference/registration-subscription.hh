/*
 Flexisip, a flexible SIP proxy server with media capabilities.
 Copyright (C) 2018 Belledonne Communications SARL.
 
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

#pragma once

#include <linphone++/linphone.hh>

#include <flexisip/registrardb.hh>


namespace flexisip {
	
/*Base for a class that manages registration information subscription for the server group chatroom*/
class RegistrationSubscription : public std::enable_shared_from_this<RegistrationSubscription>{
	public:
		RegistrationSubscription(const std::shared_ptr<linphone::ChatRoom> &cr, const std::shared_ptr<const linphone::Address> &participant);
		virtual void start() = 0;
		virtual void stop() = 0;
		virtual ~RegistrationSubscription();
		std::shared_ptr<linphone::ChatRoom> getChatRoom()const;
	protected:
		/*call this to notify the current list of participant devices for the requested participant*/
		void notify(const std::list< std::shared_ptr<linphone::Address> > & participantDevices);
		/*call this to notify that a device has just registered*/
		void notifyRegistration(const std::shared_ptr<linphone::Address>  & participantDevices);
		const std::shared_ptr<linphone::ChatRoom> mChatRoom;
		const std::shared_ptr<linphone::Address> mParticipant;
		unsigned int mChatroomRequestedCapabilities;
};

class RegistrationSubscriptionFetchListener : public ContactUpdateListener, public std::enable_shared_from_this<RegistrationSubscriptionFetchListener>{
	public:
		virtual ~RegistrationSubscriptionFetchListener() = default;
};

class RegistrationSubscriptionListener : public ContactRegisteredListener, public std::enable_shared_from_this<RegistrationSubscriptionListener>{
	public:
		virtual ~RegistrationSubscriptionListener() = default;
};

/**
 * Implementation of a registration subscription based on flexisip's RegistrarDb.
 */
class OwnRegistrationSubscription
	: public RegistrationSubscription, protected RegistrationSubscriptionFetchListener, protected RegistrationSubscriptionListener
{
	public:
		OwnRegistrationSubscription(const std::shared_ptr<linphone::ChatRoom> &cr, const std::shared_ptr<const linphone::Address> &participant);
		virtual void start() override;
		virtual void stop() override;

	private:
		unsigned int getContactCapabilities(const std::shared_ptr<ExtendedContact> &ct);
		std::shared_ptr<linphone::Address> getPubGruu(Record *r, const std::shared_ptr<ExtendedContact> &ec);
		void processRecord(Record *r);
		/*ContactUpdateListener virtual functions to override*/
		virtual void onRecordFound (Record *r) override;
		virtual void onError () override;
		virtual void onInvalid () override;
		virtual void onContactUpdated (const std::shared_ptr<ExtendedContact> &ec) override {}
		/*ContactRegisteredListener overrides*/
		virtual void onContactRegistered(Record *r, const std::string &uid) override;

		SofiaAutoHome mHome;
		const url_t *mParticipantAor;
		bool mActive;
		
};

} // namespace flexisip
