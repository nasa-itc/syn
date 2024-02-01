#include <syn_data_provider.hpp>

namespace Nos3
{
    REGISTER_DATA_PROVIDER(SynDataProvider,"SYN_PROVIDER");

    extern ItcLogger::Logger *sim_logger;

    SynDataProvider::SynDataProvider(const boost::property_tree::ptree& config) : SimIDataProvider(config)
    {
        sim_logger->trace("SynDataProvider::SynDataProvider:  Constructor executed");
        _request_count = 0;
    }

    boost::shared_ptr<SimIDataPoint> SynDataProvider::get_data_point(void) const
    {
        sim_logger->trace("SynDataProvider::get_data_point:  Executed");

        /* Prepare the provider data */
        _request_count++;

        /* Request a data point */
        SimIDataPoint *dp = new SynDataPoint(_request_count);

        /* Return the data point */
        return boost::shared_ptr<SimIDataPoint>(dp);
    }
}
