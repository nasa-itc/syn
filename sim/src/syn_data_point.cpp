#include <ItcLogger/Logger.hpp>
#include <syn_data_point.hpp>

namespace Nos3
{
    extern ItcLogger::Logger *sim_logger;

    SynDataPoint::SynDataPoint(double count)
    {
        sim_logger->trace("SynDataPoint::SynDataPoint:  Defined Constructor executed");

        /* Do calculations based on provided data */
        _syn_data_is_valid = true;
        _syn_data[0] = count * 0.001;
        _syn_data[1] = count * 0.002;
        _syn_data[2] = count * 0.003;
    }

    SynDataPoint::SynDataPoint(int16_t spacecraft, const boost::shared_ptr<Sim42DataPoint> dp) : _dp(*dp), _sc(spacecraft)
    {
        sim_logger->trace("SynDataPoint::SynDataPoint:  42 Constructor executed");

        /* Initialize data */
        _syn_data_is_valid = false;
        _syn_data[0] = _syn_data[1] = _syn_data[2] = 0.0;
    }
    
    void SynDataPoint::do_parsing(void) const
    {
        try {
            /*
            ** Declare 42 telemetry string prefix
            ** 42 variables defined in `42/Include/42types.h`
            ** 42 data stream defined in `42/Source/IPC/SimWriteToSocket.c`
            */
            std::string key;
            key.append("SC[").append(std::to_string(_sc)).append("].svb"); // SC[N].svb

            /* Parse 42 telemetry */
            std::string values = _dp.get_value_for_key(key);

            std::vector<double> data;
            parse_double_vector(values, data);

            _syn_data[0] = data[0];
            _syn_data[1] = data[1];
            _syn_data[2] = data[2];

            /* Mark data as valid */
            _syn_data_is_valid = true;

            _not_parsed = false;

            /* Debug print */
            sim_logger->trace("SynDataPoint::SynDataPoint:  Parsed svb = %f %f %f", _syn_data[0], _syn_data[1], _syn_data[2]);
        } catch (const std::exception &e) {
            sim_logger->error("SynDataPoint::SynDataPoint:  Error parsing svb.  Error=%s", e.what());
        }
    }

    /* Used for printing a representation of the data point */
    std::string SynDataPoint::to_string(void) const
    {
        sim_logger->trace("SynDataPoint::to_string:  Executed");
        
        std::stringstream ss;

        ss << std::fixed << std::setfill(' ');
        ss << "Syn Data Point:   Valid: ";
        ss << (_syn_data_is_valid ? "Valid" : "INVALID");
        ss << std::setprecision(std::numeric_limits<double>::digits10); /* Full double precision */
        ss << " Syn Data: "
           << _syn_data[0]
           << " "
           << _syn_data[1]
           << " "
           << _syn_data[2];

        return ss.str();
    }
} /* namespace Nos3 */
