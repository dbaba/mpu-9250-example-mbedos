#pragma once

#include <functional>
#include "mbed-drivers/mbed.h"
#include "mbed-drivers/v2/I2C.hpp"

namespace i2c_async {
typedef enum {
    OK = 0,
    ERROR = 1,
    NO_SLAVE = 2
} StatusCode;

using I2CCallback = std::function<void(i2c_async::StatusCode, std::size_t, uint8_t*)>; // uint8_t* must be `delete[]`d within a callback unless it's a nullptr
using I2CCallbackFunctionPointer = mbed::util::FunctionPointer2<void, mbed::drivers::v2::I2CTransaction*, uint32_t>;
using I2CNextTaskFunctionPointer = mbed::util::FunctionPointer3<void, i2c_async::StatusCode, std::size_t, uint8_t*>;
using I2CWriteByteCallback = std::function<void(i2c_async::StatusCode)>;
using I2CReadByteCallback = std::function<void(i2c_async::StatusCode, uint8_t)>;
using I2CReadBytesCallback = I2CCallback; // uint8_t* must be `delete[]`d within a callback unless it's a nullptr

template<std::size_t SIZE>
struct State {
    std::array<std::tuple<uint8_t, uint8_t, uint8_t>, SIZE> tasks;
    uint8_t pos = 0;
    State(const std::array<std::tuple<uint8_t, uint8_t, uint8_t>, SIZE>& series) : tasks(series) {}
};

class I2CAsyncOperation {

    template<std::size_t SIZE>
    struct State {
        std::array<std::tuple<uint8_t, uint8_t, uint8_t>, SIZE> tasks;
        uint8_t pos = 0;
        State(const std::array<std::tuple<uint8_t, uint8_t, uint8_t>, SIZE>& series) : tasks(series) {}
    };

    I2CCallback _callback;
    uint16_t _delay_ms;
    bool _readable;
    bool _write_before_read;

    I2CAsyncOperation(const I2CCallback &callback, uint16_t delay_ms=0, bool readable=false, bool write_before_read=true):
        _callback(callback),
        _delay_ms(delay_ms),
        _readable(readable),
        _write_before_read(write_before_read) {
    }

    template<std::size_t SIZE>
    static void do_async_write_multiple_registers(mbed::drivers::v2::I2C* i2c, uint8_t address, State<SIZE>* state, i2c_async::StatusCode status, I2CWriteByteCallback callback) {
        if ((state->pos < state->tasks.size()) && (status == i2c_async::OK)) {
            I2CWriteByteCallback wrapper = [i2c, address, callback, state](i2c_async::StatusCode status) {
                do_async_write_multiple_registers(i2c, address, state, status, callback);
            };
            std::tuple<uint8_t, uint8_t, uint8_t> t = state->tasks[state->pos];
            ++state->pos;
            async_write_single_register(i2c, address, std::get<0>(t), std::get<1>(t), wrapper, std::get<2>(t));
        } else {
            if (state) {
                delete state;
                state = nullptr;
            }
            callback(status);
        }
    }

public:
    ~I2CAsyncOperation() {
    }

    static void async_write_single_register(mbed::drivers::v2::I2C* i2c, uint8_t address, uint8_t register_address, uint8_t data, I2CWriteByteCallback callback, uint16_t delay_ms=0) {
        I2CCallback wrapper = [callback](i2c_async::StatusCode status, std::size_t, uint8_t*) {
            callback(status);
        };
        I2CAsyncOperation* write = new I2CAsyncOperation(wrapper, delay_ms, false, false);
        I2CCallbackFunctionPointer i2c_write_fp(write, &I2CAsyncOperation::i2c_callback);
        char data_write[2];
        data_write[0] = register_address;
        data_write[1] = data;
        i2c->transfer_to(address)
            .tx_ephemeral(data_write, 2)
            .on(I2C_EVENT_ALL, i2c_write_fp)
            .apply();
    }

    template<std::size_t SIZE>
    static void async_write_multiple_registers(mbed::drivers::v2::I2C* i2c, uint8_t address, const std::array<std::tuple<uint8_t, uint8_t, uint8_t>, SIZE>& series, I2CWriteByteCallback callback) {
        State<SIZE> *state = new State<SIZE>(series); // deleted at the final callback (do_async_write)
        do_async_write_multiple_registers(i2c, address, state, i2c_async::OK, callback);
    }

    static void async_read_multiple_registers(mbed::drivers::v2::I2C* i2c, uint8_t address, uint8_t register_address, uint8_t read_len, I2CReadBytesCallback callback, uint16_t delay_ms=0) {
        I2CAsyncOperation* read = new I2CAsyncOperation(callback, delay_ms, true, true);
        I2CCallbackFunctionPointer i2c_read_fp(read, &I2CAsyncOperation::i2c_callback);
        i2c->transfer_to(address)
            .tx_ephemeral(&register_address, 1)
            .rx(read_len)
            .on(I2C_EVENT_ALL, i2c_read_fp)
            .apply();
    }

    static void async_read_single_register(mbed::drivers::v2::I2C* i2c, uint8_t address, uint8_t register_address, I2CReadByteCallback callback, uint16_t delay_ms=0) {
        i2c_async::I2CCallback wrapper = [callback](i2c_async::StatusCode status, std::size_t, uint8_t* data) {
            callback(status, *data);
        };
        async_read_multiple_registers(i2c, address, register_address, 1, wrapper, delay_ms);
    }

    static void async_bulk_read(mbed::drivers::v2::I2C* i2c, uint8_t address, uint8_t read_len, I2CCallback callback, uint16_t delay_ms=0) {
        I2CAsyncOperation* read = new I2CAsyncOperation(callback, delay_ms, true, false);
        I2CCallbackFunctionPointer i2c_read_fp(read, &I2CAsyncOperation::i2c_callback);
        i2c->transfer_to(address)
            .rx(read_len)
            .on(I2C_EVENT_ALL, i2c_read_fp)
            .apply();
    }

    void minar_callback(i2c_async::StatusCode status, std::size_t rx_size, uint8_t* rx_values) {
        _callback(status, rx_size, rx_values);
        delete this; // destroy myself
    }

    void i2c_callback(mbed::drivers::v2::I2CTransaction * t, uint32_t event) {
        i2c_async::StatusCode status = i2c_async::OK;
        std::size_t rx_size = 0;
        uint8_t* rx_values = nullptr;
        mbed::drivers::v2::EphemeralBuffer* buf = nullptr;

        switch (event) {
        case I2C_EVENT_ERROR_NO_SLAVE:
            status = i2c_async::NO_SLAVE;
            break;
        case I2C_EVENT_TRANSFER_COMPLETE:
            break;
        default:
            status = i2c_async::ERROR;
        }

        if (_readable) {
            t->reset_current();
            if (_write_before_read) {
                buf = t
                    ->get_current() // get the first segment (the tx segment)
                    ->get_next();   // get the second segment (the rx segment)
            } else {
                buf = t
                    ->get_current();// get the first segment (the rx segment)
            }
            rx_size = buf->get_len();
            rx_values = (uint8_t*) buf->get_buf();    // get the buffer pointer
        }
        I2CNextTaskFunctionPointer i2c_fp(this, &I2CAsyncOperation::minar_callback);
        mbed::util::FunctionPointerBind<void> next_task(i2c_fp.bind(status, rx_size, rx_values));
        if (_delay_ms > 0) {
            minar::Scheduler::postCallback(next_task).delay(minar::milliseconds(_delay_ms));
        } else {
            minar::Scheduler::postCallback(next_task);
        }
    }
};
} // i2c_async
