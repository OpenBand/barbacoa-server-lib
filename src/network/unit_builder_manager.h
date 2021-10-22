#pragma once

#include <deque>
#include <memory>
#include <stdexcept>
#include <string>

#include <server_lib/network/unit_builder_i.h>

namespace server_lib {
namespace network {

    /**
 * class coordinating the several builders and the builder factory to build all the replies
 *
 */
    class unit_builder_manager
    {
    public:
        /**
     * ctor
     *
     */
        unit_builder_manager(const std::shared_ptr<unit_builder_i> builder = {});
        /**
     * dtor
     *
     */
        ~unit_builder_manager() = default;

        /**
    * copy ctor
     *
     */
        unit_builder_manager(const unit_builder_manager&) = delete;
        /**
    * assignment operator
     *
     */
        unit_builder_manager& operator=(const unit_builder_manager&) = delete;

    public:
        void set_builder(const std::shared_ptr<unit_builder_i> builder);

        unit_builder_i& builder();

        /**
     * add data to unit builder
     * data is used to build replies that can be retrieved with get_front later on if receive_available returns true
     *
     * \param data data to be used for building replies
     * \return current instance
     *
     */
        unit_builder_manager& operator<<(const std::string& data);

        /**
     * similar as get_front, store unit in the passed parameter
     *
     * \param unit reference to the unit object where to store the first available unit
     *
     */
        void operator>>(unit& unit);

        /**
     * \return the first available unit
     *
     */
        const unit& get_front() const;

        /**
     * pop the first available unit
     *
     */
        void pop_front();

        /**
     * \return whether a unit is available
     *
     */
        bool receive_available() const;

        /**
     * reset the unit builder to its initial state (clear internal buffer and stages)
     *
     */
        void reset();

    private:
        /**
     * build unit using _buffer content
     *
     * \return whether the unit has been fully built or not
     *
     */
        bool build_unit();

    private:
        /**
     * buffer to be used to build data
     *
     */
        std::string _buffer;

        /**
     * builder used to build replies
     *
     */
        std::shared_ptr<unit_builder_i> _builder;

        /**
     * queue of available (built) replies
     *
     */
        std::deque<unit> _available_replies;
    };

} // namespace network
} // namespace server_lib
