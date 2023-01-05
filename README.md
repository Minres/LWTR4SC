# LWTR4SC - Lightweight transaction recording for SystemC

LWTR4SC is inspired by the transaction recoridng facilities of the SystemC Verification Library (SCV).
It follows the approach there but implements it using modern C++. Therefore it requires at least C++11.

# Structure

LWTR4SC provides an API to which allows to define what to record (the 'frontend') and an API how to record it (the 'backend').

The classes and functions to define what to record are defined in lwtr.h and located in the namespace lwtr.
The hierarchy is as follows:
* a tx_db is the database holding all txfiber
* a tx_fiber is a stream of transaction (similar to a signal or wire in a wave form database) holding tx_generators
* a tx_generator is the creator of transactions and thus defines the type(s) of transactions. It can hold transaction attributes.
* a tx_hanlde represents the transaction in the database. It allows to create named relations between transactions.

The API on how to record transactions is realized using callback functions.
Those are registered using the static register_* functions of the respective frontend classes.
In particular:
* std::function<void(const tx_db&, callback_reason)> to open and close the database
* std::function<void(const tx_fiber&, callback_reason)> to create the fibers in the backend
* std::function<void(const tx_fiber&, callback_reason)> to create the generators in the backend
* std::function<void(const tx_handle&, callback_reason, value const&)> to start and end transactions with associated stat and end attributes
* std::function<void(tx_handle const&, char const*, value const&)> to record attributes independend of start and end
* std::function<void(tx_handle const&, tx_handle const&, tx_relation_handle)> to create the relationship between trnasactions in the backend

An example of a simple text format can be found at lwtr/lwtr_text.cpp
