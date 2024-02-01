#ifndef NOS3_SYNDATAPROVIDER_HPP
#define NOS3_SYNDATAPROVIDER_HPP

#include <boost/property_tree/xml_parser.hpp>
#include <ItcLogger/Logger.hpp>
#include <syn_data_point.hpp>
#include <sim_i_data_provider.hpp>

namespace Nos3
{
    class SynDataProvider : public SimIDataProvider
    {
    public:
        /* Constructors */
        SynDataProvider(const boost::property_tree::ptree& config);

        /* Accessors */
        boost::shared_ptr<SimIDataPoint> get_data_point(void) const;

    private:
        /* Disallow these */
        ~SynDataProvider(void) {};
        SynDataProvider& operator=(const SynDataProvider&) {return *this;};

        mutable double _request_count;
    };
}

#endif
