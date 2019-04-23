/*
 * (c) 2005-2017  Copyright, Real-Time Innovations, Inc. All rights reserved.
 * Subject to Eclipse Public License v1.0; see LICENSE.md for details.
 */

#include "PerftestTransport.h"

/******************************************************************************/

#if defined(RTI_WIN32)
  #pragma warning(push)
  #pragma warning(disable : 4996)
  #define STRNCASECMP _strnicmp
#elif defined(RTI_VXWORKS)
  #define STRNCASECMP strncmp
#else
  #define STRNCASECMP strncasecmp
#endif
#define IS_OPTION(str, option) (STRNCASECMP(str, option, strlen(str)) == 0)

/******************************************************************************/

// Tag used when adding logging output.
const std::string classLoggingString = "PerftestTransport:";

std::map<std::string, TransportConfig> PerftestTransport::transportConfigMap;

/******************************************************************************/
/* DDS Related functions. These functions doesn't belong to the
 * PerftestTransport class. This way we decouple the Transport class from
 * the specific implementation (C++ Classic vs C++PSM).
 */

bool addPropertyToParticipantQos(
        DDS_DomainParticipantQos &qos,
        std::string propertyName,
        std::string propertyValue)
{

    DDS_ReturnCode_t retcode = DDSPropertyQosPolicyHelper::add_property(
            qos.property,
            propertyName.c_str(),
            propertyValue.c_str(),
            false);
    if (retcode != DDS_RETCODE_OK) {
        fprintf(
                stderr,
                "%s Failed to add property \"%s\"\n",
                classLoggingString.c_str(),
                propertyName.c_str());
        return false;
    }

    return true;
}

bool assertPropertyToParticipantQos(
        DDS_DomainParticipantQos &qos,
        std::string propertyName,
        std::string propertyValue)
{
    DDS_ReturnCode_t retcode = DDSPropertyQosPolicyHelper::assert_property(
            qos.property,
            propertyName.c_str(),
            propertyValue.c_str(),
            false);
    if (retcode != DDS_RETCODE_OK) {
        fprintf(
                stderr,
                "%s Failed to add property \"%s\"\n",
                classLoggingString.c_str(),
                propertyName.c_str());
        return false;
    }

    return true;
}

bool setAllowInterfacesList(
        PerftestTransport &transport,
        DDS_DomainParticipantQos &qos,
        ParameterManager *_PM)
{
    if (!_PM->get<std::string>("allowInterfaces").empty()) {

        if (transport.transportConfig.kind == TRANSPORT_NOT_SET) {
            fprintf(stderr,
                    "%s Ignoring -nic/-allowInterfaces option\n",
                    classLoggingString.c_str());
            return true;
        }

        /*
         * In these 2 specific cases, we are forced to add 2 properties, one
         * for UDPv4 and another for UDPv6
         */
        if (transport.transportConfig.kind == TRANSPORT_UDPv4_UDPv6_SHMEM
                || transport.transportConfig.kind == TRANSPORT_UDPv4_UDPv6) {

            std::string propertyName =
                    "dds.transport.UDPv4.builtin.parent.allow_interfaces";

            if (!addPropertyToParticipantQos(
                    qos,
                    propertyName,
                    _PM->get<std::string>("allowInterfaces"))) {
                return false;
            }

            propertyName =
                    "dds.transport.UDPv6.builtin.parent.allow_interfaces";

            if (!addPropertyToParticipantQos(
                    qos,
                    propertyName,
                    _PM->get<std::string>("allowInterfaces"))) {
                return false;
            }

        } else {

            std::string propertyName = transport.transportConfig.prefixString;
            if (transport.transportConfig.kind == TRANSPORT_WANv4) {
                propertyName += ".parent";
            }

            propertyName += ".parent.allow_interfaces";

            return addPropertyToParticipantQos(
                    qos,
                    propertyName,
                    _PM->get<std::string>("allowInterfaces"));
        }
    }

    return true;
}

bool setTransportVerbosity(
        PerftestTransport &transport,
        DDS_DomainParticipantQos &qos,
        ParameterManager *_PM)
{
    if (!_PM->get<std::string>("transportVerbosity").empty()) {
        if (transport.transportConfig.kind == TRANSPORT_NOT_SET) {
            fprintf(stderr,
                    "%s Ignoring -transportVerbosity option\n",
                    classLoggingString.c_str());
            return true;
        }

        std::string propertyName = transport.transportConfig.prefixString
                + ".verbosity";

        // The name of the property in TCPv4 is different
        if (transport.transportConfig.kind == TRANSPORT_TCPv4) {
            propertyName = transport.transportConfig.prefixString
                    + ".logging_verbosity_bitmap";
        } else if (transport.transportConfig.kind == TRANSPORT_UDPv4
                || transport.transportConfig.kind == TRANSPORT_UDPv6
                || transport.transportConfig.kind == TRANSPORT_SHMEM
                || transport.transportConfig.kind == TRANSPORT_UDPv4_SHMEM
                || transport.transportConfig.kind == TRANSPORT_UDPv6_SHMEM
                || transport.transportConfig.kind == TRANSPORT_UDPv4_UDPv6
                || transport.transportConfig.kind == TRANSPORT_UDPv4_UDPv6_SHMEM) {
            // We do not change logging for the builtin transports.
            return true;
        }

        return addPropertyToParticipantQos(
                qos,
                propertyName,
                _PM->get<std::string>("transportVerbosity"));
    }
    return true;
}

bool configureSecurityFiles(
        PerftestTransport &transport,
        DDS_DomainParticipantQos& qos,
        ParameterManager *_PM)
{

    if (!_PM->get<std::string>("transportCertAuthority").empty()) {
        if (!addPropertyToParticipantQos(
                qos,
                transport.transportConfig.prefixString + ".tls.verify.ca_file",
                _PM->get<std::string>("transportCertAuthority"))) {
            return false;
        }
    }

    if (!_PM->get<std::string>("transportCertFile").empty()) {
        if (!addPropertyToParticipantQos(
                qos,
                transport.transportConfig.prefixString
                        + ".tls.identity.certificate_chain_file",
                _PM->get<std::string>("transportCertFile"))) {
            return false;
        }
    }

    if (!_PM->get<std::string>("transportPrivateKey").empty()) {
        if (!addPropertyToParticipantQos(
                qos,
                transport.transportConfig.prefixString
                        + ".tls.identity.private_key_file",
                _PM->get<std::string>("transportPrivateKey"))) {
            return false;
        }
    }

    return true;
}

bool configureTcpTransport(
        PerftestTransport &transport,
        DDS_DomainParticipantQos& qos,
        ParameterManager *_PM)
{

    qos.transport_builtin.mask = DDS_TRANSPORTBUILTIN_MASK_NONE;

    if (!addPropertyToParticipantQos(
            qos,
            std::string("dds.transport.load_plugins"),
            transport.transportConfig.prefixString)) {
        return false;
    }

    if (!_PM->get<std::string>("transportServerBindPort").empty()) {
        if (!addPropertyToParticipantQos(
                qos,
                transport.transportConfig.prefixString + ".server_bind_port",
                _PM->get<std::string>("transportServerBindPort"))) {
            return false;
        }
    }

    if (_PM->get<bool>("transportWan")) {
        if (!assertPropertyToParticipantQos(
                qos,
                transport.transportConfig.prefixString
                        + ".parent.classid",
                "NDDS_TRANSPORT_CLASSID_TCPV4_WAN")) {
            return false;
        }

        if (_PM->get<std::string>("transportServerBindPort") != "0") {
            if (!_PM->get<std::string>("transportPublicAddress").empty()) {
                if (!addPropertyToParticipantQos(
                        qos,
                        transport.transportConfig.prefixString
                                + ".public_address",
                        _PM->get<std::string>("transportPublicAddress"))) {
                    return false;
                }
            } else {
                fprintf(stderr,
                        "%s Public Address is required if "
                        "Server Bind Port != 0\n",
                        classLoggingString.c_str());
                return false;
            }
        }
    }

    if (transport.transportConfig.kind == TRANSPORT_TLSv4) {
        if (_PM->get<bool>("transportWan")) {
            if (!assertPropertyToParticipantQos(
                    qos,
                    transport.transportConfig.prefixString
                            + ".parent.classid",
                    "NDDS_TRANSPORT_CLASSID_TLSV4_WAN")) {
                return false;
            }
        } else {
            if (!assertPropertyToParticipantQos(
                    qos,
                    transport.transportConfig.prefixString
                            + ".parent.classid",
                    "NDDS_TRANSPORT_CLASSID_TLSV4_LAN")) {
                return false;
            }
        }

        if (!configureSecurityFiles(transport, qos, _PM)) {
            return false;
        }
    }

    return true;
}

bool configureDtlsTransport(
        PerftestTransport &transport,
        DDS_DomainParticipantQos& qos,
        ParameterManager *_PM)
{

    qos.transport_builtin.mask = DDS_TRANSPORTBUILTIN_MASK_NONE;

    if (!addPropertyToParticipantQos(
            qos,
            std::string("dds.transport.load_plugins"),
            transport.transportConfig.prefixString)) {
        return false;
    }

    if (!addPropertyToParticipantQos(
            qos,
            transport.transportConfig.prefixString + ".library",
            "nddstransporttls")) {
        return false;
    }

    if (!addPropertyToParticipantQos(
            qos,
            transport.transportConfig.prefixString + ".create_function",
            "NDDS_Transport_DTLS_create")) {
        return false;
    }

    if (!configureSecurityFiles(transport, qos, _PM)) {
        return false;
    }

    return true;
}

bool configureWanTransport(
        PerftestTransport &transport,
        DDS_DomainParticipantQos& qos,
        ParameterManager *_PM)
{

    qos.transport_builtin.mask = DDS_TRANSPORTBUILTIN_MASK_NONE;

    if (!addPropertyToParticipantQos(
            qos,
            std::string("dds.transport.load_plugins"),
            transport.transportConfig.prefixString)) {
        return false;
    }

    if (!addPropertyToParticipantQos(
            qos,
            transport.transportConfig.prefixString + ".library",
            "nddstransportwan")) {
        return false;
    }

    if (!addPropertyToParticipantQos(
            qos,
            transport.transportConfig.prefixString + ".create_function",
            "NDDS_Transport_WAN_create")) {
        return false;
    }

    if (!_PM->get<std::string>("transportWanServerAddress").empty()) {
        if (!addPropertyToParticipantQos(
                qos,
                transport.transportConfig.prefixString + ".server",
                _PM->get<std::string>("transportWanServerAddress"))) {
            return false;
        }
    } else {
        fprintf(stderr,
                "%s Wan Server Address is required\n",
                classLoggingString.c_str());
        return false;
    }

    if (!_PM->get<std::string>("transportWanServerPort").empty()) {
        if (!addPropertyToParticipantQos(
                qos,
                transport.transportConfig.prefixString + ".server_port",
                _PM->get<std::string>("transportWanServerPort"))) {
            return false;
        }
    }

    if (!_PM->get<std::string>("transportWanId").empty()) {
        if (!addPropertyToParticipantQos(
                qos,
                transport.transportConfig.prefixString
                        + ".transport_instance_id",
                _PM->get<std::string>("transportWanId"))) {
            return false;
        }
    } else {
        fprintf(stderr, "%s Wan ID is required\n", classLoggingString.c_str());
        return false;
    }

    if (_PM->get<bool>("transportSecureWan")) {
        if (!addPropertyToParticipantQos(
                qos,
                transport.transportConfig.prefixString + ".enable_security",
                "1")) {
            return false;
        }

        if (!configureSecurityFiles(transport, qos, _PM)) {
            return false;
        }
    }

    return true;
}

bool configureShmemTransport(
        PerftestTransport &transport,
        DDS_DomainParticipantQos& qos,
        ParameterManager *_PM)
{
    // Number of messages that can be buffered in the receive queue.
    int received_message_count_max = 1024 * 1024 * 2
            / (int) _PM->get<unsigned long long>("dataLen");
    if (received_message_count_max < 1) {
        received_message_count_max = 1;
    }

    std::ostringstream string_stream_object;
    string_stream_object << received_message_count_max;
    if (!assertPropertyToParticipantQos(
            qos,
            "dds.transport.shmem.builtin.received_message_count_max",
            string_stream_object.str())) {
        return false;
    }
    return true;
}

bool configureTransport(
        PerftestTransport &transport,
        DDS_DomainParticipantQos &qos,
        ParameterManager *_PM)
{

    /*
     * If transportConfig.kind is not set, then we want to use the value
     * provided by the Participant Qos, so first we read it from there and
     * update the value of transportConfig.kind with whatever was already set.
     */

    if (transport.transportConfig.kind == TRANSPORT_NOT_SET) {

        DDS_TransportBuiltinKindMask mask = qos.transport_builtin.mask;
        switch (mask) {
            case DDS_TRANSPORTBUILTIN_UDPv4:
                transport.transportConfig = TransportConfig(
                    TRANSPORT_UDPv4,
                    "UDPv4",
                    "dds.transport.UDPv4.builtin");
                break;
            case DDS_TRANSPORTBUILTIN_UDPv6:
                transport.transportConfig = TransportConfig(
                    TRANSPORT_UDPv6,
                    "UDPv6",
                    "dds.transport.UDPv6.builtin");
                break;
            case DDS_TRANSPORTBUILTIN_SHMEM:
                transport.transportConfig = TransportConfig(
                    TRANSPORT_SHMEM,
                    "SHMEM",
                    "dds.transport.shmem.builtin");
                break;
            case DDS_TRANSPORTBUILTIN_UDPv4|DDS_TRANSPORTBUILTIN_SHMEM:
                transport.transportConfig = TransportConfig(
                    TRANSPORT_UDPv4_SHMEM,
                    "UDPv4 & SHMEM",
                    "dds.transport.UDPv4.builtin");
                break;
            case DDS_TRANSPORTBUILTIN_UDPv4|DDS_TRANSPORTBUILTIN_UDPv6:
                transport.transportConfig = TransportConfig(
                    TRANSPORT_UDPv4_UDPv6,
                    "UDPv4 & UDPv6",
                    "dds.transport.UDPv4.builtin");
                break;
            case DDS_TRANSPORTBUILTIN_SHMEM|DDS_TRANSPORTBUILTIN_UDPv6:
                transport.transportConfig = TransportConfig(
                    TRANSPORT_UDPv6_SHMEM,
                    "UDPv6 & SHMEM",
                    "dds.transport.UDPv6.builtin");
                break;
            case DDS_TRANSPORTBUILTIN_UDPv4|DDS_TRANSPORTBUILTIN_UDPv6|DDS_TRANSPORTBUILTIN_SHMEM:
                transport.transportConfig = TransportConfig(
                    TRANSPORT_UDPv4_UDPv6_SHMEM,
                    "UDPv4 & UDPv6 & SHMEM",
                    "dds.transport.UDPv4.builtin");
                break;
            default:
                /*
                 * This would mean that the mask is either empty or a
                 * different value that we do not support yet. So we keep
                 * the value as "TRANSPORT_NOT_SET"
                 */
                break;
        }
        transport.transportConfig.takenFromQoS = true;
    }

    switch (transport.transportConfig.kind) {

        case TRANSPORT_UDPv4:
            qos.transport_builtin.mask = DDS_TRANSPORTBUILTIN_UDPv4;
            break;

        case TRANSPORT_UDPv6:
            qos.transport_builtin.mask = DDS_TRANSPORTBUILTIN_UDPv6;
            break;

        case TRANSPORT_SHMEM:
            qos.transport_builtin.mask = DDS_TRANSPORTBUILTIN_SHMEM;
            if (!configureShmemTransport(transport, qos, _PM)) {
                fprintf(stderr,
                        "%s Failed to configure SHMEM plugin\n",
                        classLoggingString.c_str());
                return false;
            }
            break;

        case TRANSPORT_TCPv4:
            if (!configureTcpTransport(transport, qos, _PM)) {
                fprintf(stderr,
                        "%s Failed to configure TCP plugin\n",
                        classLoggingString.c_str());
                return false;
            }
            break;

        case TRANSPORT_TLSv4:
            if (!configureTcpTransport(transport, qos, _PM)) {
                fprintf(stderr,
                        "%s Failed to configure TCP + TLS plugin\n",
                        classLoggingString.c_str());
                return false;
            }
            break;

        case TRANSPORT_DTLSv4:
            if (!configureDtlsTransport(transport, qos, _PM)) {
                fprintf(stderr,
                        "%s Failed to configure DTLS plugin\n",
                        classLoggingString.c_str());
                return false;
            }
            break;

        case TRANSPORT_WANv4:
            if (!configureWanTransport(transport, qos, _PM)) {
                fprintf(stderr,
                        "%s Failed to configure WAN plugin\n",
                        classLoggingString.c_str());
                return false;
            }
            break;
        default:

            /*
             * If shared memory is enabled we want to set up its
             * specific configuration
             */
            if ((qos.transport_builtin.mask & DDS_TRANSPORTBUILTIN_SHMEM)
                    != 0) {
                if (!configureShmemTransport(transport, qos, _PM)) {
                    fprintf(stderr,
                            "%s Failed to configure SHMEM plugin\n",
                            classLoggingString.c_str());
                    return false;
                }
            }
            break;
    } // Switch

    /*
     * If the transport is empty or if it is shmem, it does not make sense
     * setting an interface, in those cases, if the allow interfaces is provided
     * we empty it.
     */
    if (transport.transportConfig.kind != TRANSPORT_NOT_SET
            && transport.transportConfig.kind != TRANSPORT_SHMEM) {
        if (!setAllowInterfacesList(transport, qos, _PM)) {
            return false;
        }
    } else {
        // We are not using the allow interface string, so it is clean
        _PM->set<std::string>("allowInterfaces", std::string(""));
    }

    if (!setTransportVerbosity(transport, qos, _PM)) {
        return false;
    }

    return true;
}

/******************************************************************************/
/* CLASS CONSTRUCTOR AND DESTRUCTOR */

PerftestTransport::PerftestTransport()
{
    multicastAddrMap[LATENCY_TOPIC_NAME] = TRANSPORT_MULTICAST_ADDR_LATENCY;
    multicastAddrMap[ANNOUNCEMENT_TOPIC_NAME] =
            TRANSPORT_MULTICAST_ADDR_ANNOUNCEMENT;
    multicastAddrMap[THROUGHPUT_TOPIC_NAME] =
            TRANSPORT_MULTICAST_ADDR_THROUGHPUT;
}

PerftestTransport::~PerftestTransport()
{
}

void PerftestTransport::initialize(ParameterManager *PM)
{
    _PM = PM;
}

/******************************************************************************/
/* PRIVATE METHODS */

const std::map<std::string, TransportConfig>&
PerftestTransport::getTransportConfigMap()
{

    if (transportConfigMap.empty()) {
        transportConfigMap["Use XML"] = TransportConfig(
                TRANSPORT_NOT_SET,
                "--",
                "--");
        transportConfigMap["UDPv4"] = TransportConfig(
                TRANSPORT_UDPv4,
                "UDPv4",
                "dds.transport.UDPv4.builtin");
        transportConfigMap["UDPv6"] = TransportConfig(
                TRANSPORT_UDPv6,
                "UDPv6",
                "dds.transport.UDPv6.builtin");
        transportConfigMap["TCP"] = TransportConfig(
                TRANSPORT_TCPv4,
                "TCP",
                "dds.transport.TCPv4.tcp1");
        transportConfigMap["TLS"] = TransportConfig(
                TRANSPORT_TLSv4,
                "TLS",
                "dds.transport.TCPv4.tcp1");
        transportConfigMap["DTLS"] = TransportConfig(
                TRANSPORT_DTLSv4,
                "DTLS",
                "dds.transport.DTLS.dtls1");
        transportConfigMap["WAN"] = TransportConfig(
                TRANSPORT_WANv4,
                "WAN",
                "dds.transport.WAN.wan1");
        transportConfigMap["SHMEM"] = TransportConfig(
                TRANSPORT_SHMEM,
                "SHMEM",
                "dds.transport.shmem.builtin");
    }
    return transportConfigMap;
}

bool PerftestTransport::setTransport(std::string transportString)
{

    std::map<std::string, TransportConfig> configMap = getTransportConfigMap();
    std::map<std::string, TransportConfig>::iterator it = configMap.find(
            transportString);
    if (it != configMap.end()) {
        transportConfig = it->second;
    } else {
        fprintf(stderr,
                "%s \"%s\" is not a valid transport. "
                "List of supported transport:\n",
                classLoggingString.c_str(),
                transportString.c_str());
        for (std::map<std::string, TransportConfig>::iterator it = configMap
                .begin(); it != configMap.end(); ++it) {
            fprintf(stderr, "\t\"%s\"\n", it->first.c_str());
        }
        return false;
    }
    return true;
}

void PerftestTransport::populateSecurityFiles() {

    if (_PM->get<std::string>("transportCertFile").empty()) {
        if (_PM->get<bool>("pub")) {
            _PM->set("transportCertFile", TRANSPORT_CERTIFICATE_FILE_PUB);
        } else {
            _PM->set("transportCertFile", TRANSPORT_CERTIFICATE_FILE_SUB);
        }
    }

    if (_PM->get<std::string>("transportPrivateKey").empty()) {
        if (_PM->get<bool>("pub")) {
            _PM->set("transportPrivateKey", TRANSPORT_PRIVATEKEY_FILE_PUB);
        } else {
            _PM->set("transportPrivateKey", TRANSPORT_PRIVATEKEY_FILE_SUB);
        }
    }

    if (_PM->get<std::string>("transportCertAuthority").empty()) {
        _PM->set("transportCertAuthority", TRANSPORT_CERTAUTHORITY_FILE);
    }
}

/******************************************************************************/
/* PUBLIC METHODS */

std::string PerftestTransport::printTransportConfigurationSummary()
{
    std::ostringstream stringStream;
    stringStream << "Transport Configuration:\n";
    stringStream << "\tKind: " << transportConfig.nameString;
    if (transportConfig.takenFromQoS) {
        stringStream << " (taken from QoS XML file)";
    }
    stringStream << "\n";

    if (!_PM->get<std::string>("allowInterfaces").empty()) {
        stringStream << "\tNic: "
                     << _PM->get<std::string>("allowInterfaces")
                     << "\n";
    }

    stringStream << "\tUse Multicast: "
                 << ((allowsMulticast() && _PM->get<bool>("multicast"))
                        ? "True" : "False");
    if (!allowsMulticast() && _PM->get<bool>("multicast")) {
        stringStream << " (Multicast is not supported for "
                     << transportConfig.nameString << ")";
    }
    stringStream << "\n";

    if (_PM->is_set("multicastAddr")) {
        stringStream << "\tUsing custom Multicast Addresses:"
                << "\n\t\tThroughtput Address: "
                << multicastAddrMap[THROUGHPUT_TOPIC_NAME].c_str()
                << "\n\t\tLatency Address: "
                << multicastAddrMap[LATENCY_TOPIC_NAME].c_str()
                << "\n\t\tAnnouncement Address: "
                << multicastAddrMap[ANNOUNCEMENT_TOPIC_NAME].c_str();
    }

    if (transportConfig.kind == TRANSPORT_TCPv4
            || transportConfig.kind == TRANSPORT_TLSv4) {
        stringStream << "\tTCP Server Bind Port: "
                     << _PM->get<std::string>("transportServerBindPort")
                     << "\n";

        stringStream << "\tTCP LAN/WAN mode: "
                     << (_PM->get<bool>("transportWan") ? "WAN\n" : "LAN\n");
        if (_PM->get<bool>("transportWan")) {
            stringStream << "\tTCP Public Address: "
                         << _PM->get<std::string>("transportPublicAddress")
                         << "\n";
        }
    }

    if (transportConfig.kind == TRANSPORT_WANv4) {
        stringStream << "\tWAN Server Address: "
                     << _PM->get<std::string>("transportWanServerAddress")
                     << ":"
                     << _PM->get<std::string>("transportWanServerPort")
                     << "\n";
        stringStream << "\tWAN Id: "
                     << _PM->get<std::string>("transportWanId")
                     << "\n";
        stringStream << "\tWAN Secure: "
                     << _PM->get<bool>("transportSecureWan")
                     << "\n";
    }

    if (transportConfig.kind == TRANSPORT_TLSv4
            || transportConfig.kind == TRANSPORT_DTLSv4
            || (transportConfig.kind == TRANSPORT_WANv4
            && _PM->get<bool>("transportSecureWan"))) {
        stringStream << "\tCertificate authority file: "
                     << _PM->get<std::string>("transportCertAuthority")
                     << "\n";
        stringStream << "\tCertificate file: "
                     << _PM->get<std::string>("transportCertFile")
                     << "\n";
        stringStream << "\tPrivate key file: "
                     << _PM->get<std::string>("transportPrivateKey")
                     << "\n";
    }

    if (!_PM->get<std::string>("transportVerbosity").empty()) {
        stringStream << "\tVerbosity: "
                     << _PM->get<std::string>("transportVerbosity")
                     << "\n";
    }

    return stringStream.str();
}

/*********************************************************
 * Validate and manage the parameterS
 */
bool PerftestTransport::validate_input()
{
    /*
     * Manage parameter -allowInterfaces -nic
     * "-allowInterfaces" and "-nic" are the same parameter,
     * so now use only one: "allowInterfaces"
     */
    if (_PM->get<std::string>("allowInterfaces").empty()) {
        _PM->set<std::string>("allowInterfaces", _PM->get<std::string>("nic"));
    }

    // Manage parameter -transport
    if (!setTransport(_PM->get<std::string>("transport"))) {
        fprintf(stderr,
                "%s Error Setting the transport\n",
                classLoggingString.c_str());
        return false;
    }

    // Manage parameter -multicastAddr
    if(_PM->is_set("multicastAddr")) {
        if (!parse_multicast_addresses(
                _PM->get<std::string>("multicastAddr").c_str())) {
            fprintf(stderr, "Error parsing -multicastAddr\n");
            return false;
        }
        _PM->set<bool>("multicast", true);
    }

    // We only need to set the secure properties for this
    if (transportConfig.kind == TRANSPORT_TLSv4
            || transportConfig.kind == TRANSPORT_DTLSv4
            || transportConfig.kind == TRANSPORT_WANv4) {
        populateSecurityFiles();
    }

    return true;
}

bool PerftestTransport::allowsMulticast()
{
    return (transportConfig.kind != TRANSPORT_TCPv4
            && transportConfig.kind != TRANSPORT_TLSv4
            && transportConfig.kind != TRANSPORT_WANv4
            && transportConfig.kind != TRANSPORT_SHMEM);
}

const std::string PerftestTransport::getMulticastAddr(const char *topicName)
{
    std::string address = multicastAddrMap[std::string(topicName)];

    if (address.length() == 0) {
        return NULL;
    }

    return address;
}

bool PerftestTransport::is_multicast(std::string addr)
{

    NDDS_Transport_Address_t transportAddress;

    if (!NDDS_Transport_Address_from_string(&transportAddress, addr.c_str())) {
        fprintf(stderr, "Fail to get a transport address from string\n");
        return false;
    }

    return NDDS_Transport_Address_is_multicast(&transportAddress);
}

bool PerftestTransport::increase_address_by_one(
        const std::string addr,
        std::string &nextAddr)
{
    bool success = false;
    bool isIPv4 = false; /* true = IPv4 // false = IPv6 */
    NDDS_Transport_Address_t transportAddress;
    char buffer[NDDS_TRANSPORT_ADDRESS_STRING_BUFFER_SIZE];

    if (!NDDS_Transport_Address_from_string(&transportAddress, addr.c_str())) {
        fprintf(stderr, "Fail to get a transport address from string\n");
        return false;
    }

    isIPv4 = NDDS_Transport_Address_is_ipv4(&transportAddress);

    /*
     * Increase the address by one value.
     * If the Address is 255.255.255.255 (or the equivalent for IPv6) this
     * function will FAIL
     */
    for (int i = NDDS_TRANSPORT_ADDRESS_LENGTH - 1; i >= 0 && !success; i--) {
        if (isIPv4 && i < 9) {
            /* If the user set a IPv4 higher than 255.255.255.253 fail here*/
            break;
        }
        if (transportAddress.network_ordered_value[i] == 255) {
            transportAddress.network_ordered_value[i] = 0;
        }
        else {
            /* Increase the value and exit */
            transportAddress.network_ordered_value[i]++;
            success = true;
        }
    }

    if (!success) {
        fprintf(stderr,
                "IP value too high. Please use -help for more information "
                "about -multicastAddr command line\n");
        return false;
    }

    /* Try to get a IP string format */
    if (!NDDS_Transport_Address_to_string_with_protocol_family_format(
            &transportAddress,
            buffer,
            NDDS_TRANSPORT_ADDRESS_STRING_BUFFER_SIZE,
            isIPv4 ? RTI_OSAPI_SOCKET_AF_INET : RTI_OSAPI_SOCKET_AF_INET6)) {
        fprintf(stderr, "Fail to retrieve a ip string format\n");
        return false;
    }

    nextAddr = buffer;

    return true;
}

bool PerftestTransport::parse_multicast_addresses(const char *arg)
{
    char throughput[NDDS_TRANSPORT_ADDRESS_STRING_BUFFER_SIZE];
    char latency[NDDS_TRANSPORT_ADDRESS_STRING_BUFFER_SIZE];
    char annonuncement[NDDS_TRANSPORT_ADDRESS_STRING_BUFFER_SIZE];
    unsigned int numberOfAddressess = 0;

    /* Get number of addresses on the string */
    if (!NDDS_Transport_get_number_of_addresses_in_string(
            arg,
            &numberOfAddressess)) {
        fprintf(stderr, "Error parsing Address for -multicastAddr option\n");
        return false;
    }

    /* If three addresses are given */
    if (numberOfAddressess == 3) {
        if (!NDDS_Transport_get_address(arg, 0, throughput)
                || !NDDS_Transport_get_address(arg, 1, latency)
                || !NDDS_Transport_get_address(arg, 2, annonuncement)){
            fprintf(stderr,
                    "Error parsing Address for -multicastAddr option\n");
            return false;
        }

        multicastAddrMap[THROUGHPUT_TOPIC_NAME] = throughput;
        multicastAddrMap[LATENCY_TOPIC_NAME] = latency;
        multicastAddrMap[ANNOUNCEMENT_TOPIC_NAME] = annonuncement;

    } else if (numberOfAddressess == 1) {
        /* If only one address is given */
        if (!NDDS_Transport_get_address(arg, 0, throughput)) {
            fprintf(stderr,
                    "Error parsing Address for -multicastAddr option\n");
            return false;
        }
        multicastAddrMap[THROUGHPUT_TOPIC_NAME] = throughput;

        /* Calculate the consecutive one */
        if (!increase_address_by_one(
                multicastAddrMap[THROUGHPUT_TOPIC_NAME],
                multicastAddrMap[LATENCY_TOPIC_NAME])) {
            fprintf(stderr,
                    "Fail to increase the value of IP address given\n");
            return false;
        }

        /* Calculate the consecutive one */
        if (!increase_address_by_one(
                multicastAddrMap[LATENCY_TOPIC_NAME],
                multicastAddrMap[ANNOUNCEMENT_TOPIC_NAME])) {
            fprintf(stderr,
                    "Fail to increase the value of IP address given\n");
            return false;
        }

    } else {
        fprintf(stderr,
                "Error parsing Address/es '%s' for -multicastAddr option\n"
                "Use -help option to see the correct sintax\n",
                arg);
        return false;
    }

    /* Last check. All the IPs must be in IP format and multicast range */
    if (!is_multicast(multicastAddrMap[THROUGHPUT_TOPIC_NAME])
            || !is_multicast(multicastAddrMap[LATENCY_TOPIC_NAME])
            || !is_multicast(multicastAddrMap[ANNOUNCEMENT_TOPIC_NAME])) {

        fprintf(stderr,
                "Error parsing the address/es '%s' for -multicastAddr option\n",
                arg);
        if (numberOfAddressess == 1) {
            fprintf(stderr,
                    "\tThe calculated addresses are outsite of multicast range.\n");
        } else {
            fprintf(stderr, "\tThere are outsite of multicast range.\n");
        }

        fprintf(stderr, "\tUse -help option to see the correct sintax\n");

        return false;
    }

    return true;
}
/******************************************************************************/
