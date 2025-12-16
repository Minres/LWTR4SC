# LWTR4SC - Lightweight transaction recording for SystemC

LWTR4SC is inspired by the transaction recording facilities of the SystemC Verification Library (SCV).
It follows the approach there but implements it using modern C++. Therefore it requires at least C++11.

# Structure

LWTR4SC provides an API to define what to record (the "frontend") and how to record it (the "backend").

The frontend classes and functions to define what to record are defined in lwtr.h and is located in the namespace lwtr. 
The hierarchy is as follows:
* a tx_db is the database holding all tx_fiber.
* a tx_fiber is a stream of transaction (similar to a signal or wire in a wave form database) holding tx_generators.
* a tx_generator creates transactions and defines their types. It can hold transaction attributes.
* a tx_handle represents the transaction in the database. It allows to create named relations between transactions.

The API on how to record transactions is realized using callback functions.
Those are registered using the static register_* functions of the respective frontend classes.
In particular:
* std::function<void(const tx_db&, callback_reason)> to open and close the database
* std::function<void(const tx_fiber&, callback_reason)> to create the fibers in the backend
* std::function<void(const tx_fiber&, callback_reason)> to create the generators in the backend
* std::function<void(const tx_handle&, callback_reason, value const&)> to start and end transactions with associated start and end attributes
* std::function<void(tx_handle const&, char const*, value const&)> to record attributes independent of start and end
* std::function<void(tx_handle const&, tx_handle const&, tx_relation_handle)> to create the relationship between transactions in the backend

The library supports recording transactions in two formats. 
The first is a simple text format that can be found at lwtr/lwtr_text.cpp. 
The second is a new binary format called '**F**ast **T**ransaction **R**ecording'.

# **F**ast **T**ransaction **R**ecording (FTR) format description

FTR uses Concise Binary Object Representation (CBOR) according to RFC 8949 as the storage encoding.
CBOR documentation and related information can be found at [cbor.io](https://cbor.io/).

FTR consists of a sequence of chunks starting with an info chunk.
Each chunk consists of a CBOR tag, a header and a payload where the payload may be compressed using lz4.

The following chunks are used within a FTR database.

## info chunk

The info chunk is denoted by CBOR tag 6 followed by an array having 2 entries:

* integer denoting the timescale
  
  The timescale is encoded as the exponent of the multiplier for all timestamps.
  All timestamps in the FTR are multiples of this.
  E.g. a value of -6 means a timescale multiplier of 1e-6 or 1µs. 
  
* an epoch denoting creation time

  The [epoch](https://www.rfc-editor.org/rfc/rfc8949.html#epochdatetimesect) is a CBOR type and encoded as a tagged float or integer.
  This value denotes the time of creation of the FTR and is independent of the file or transmission date
  
  
## dictionary chunk

The dictionary chunk provides a mapping between numeric ids and strings to save space.
There can be several dictionary chunks in a file since strings need to be defined before referencing them.
All chunks form a single map, keys are guaranteed to be unique, they might be consecutive numbers.

The uncompressed dictionary chunk is denoted be CBOR tag 8 followed by an [encoded CBOR data item](https://www.rfc-editor.org/rfc/rfc8949.html#embedded-di).

The compressed dictionary chunk is denoted by CBOR tag 9 followed by an array having 2 entries:

* unsigned integer denoting the uncompressed size of the content 
* encoded CBOR data item (a byte string) holding the LZ4 compressed content of the dictionary

The data item itself is an [indefinite length map](https://www.rfc-editor.org/rfc/rfc8949.html#name-indefinite-length-arrays-an) (major type 5) having a pairs of unsigned and string as entries.

## directory chunk

The uncompressed directory chunk is denoted be CBOR tag 10 followed by an [encoded CBOR data item](https://www.rfc-editor.org/rfc/rfc8949.html#embedded-di) holding the content.

The compressed directory chunk is denoted by CBOR tag 11 followed by an array having 2 entries:

* unsigned integer denoting the uncompressed size of the   
* encoded CBOR data item (a byte string) holding the LZ4 compressed content of the directory

The directory content itself is structured as an indefinite-length array consisting of:

* a stream entry denoted by CBOR tag 16 followed by an array of size 3:
  
    * unsigned integer denoting the id
    * unsigned integer denoting name of the stream (as dictionary id)
    * unsigned integer denoting kind (as dictionary id)
  
* a generator entry denoted by CBOR tag 17 followed by an array of size 3:
  
    * unsigned integer denoting the id
    * unsigned integer denoting name of the stream (as dictionary id)
    * unsigned integer denoting stream the generator belongs to

## tx block chunk

The uncompressed tx block chunk is denoted be CBOR tag 12 followed by an array having 4 entries:

* unsigned integer denoting the stream id this block belongs to
* unsigned integer denoting the start_time (a time stamp)
* unsigned integer denoting the end_time (a time stamp)
* encoded CBOR data item (a byte string) holding the content of the tx block

The compressed tx block chunk is denoted by CBOR tag 13 followed by an array having 5 entries:

* unsigned integer denoting the stream id this block belongs to
* unsigned integer denoting the start_time (a time stamp)
* unsigned integer denoting the end_time (a time stamp)
* unsigned integer denoting the uncompressed size of the content
* encoded CBOR data item (a byte string) holding the LZ4 compressed content of the tx block

Indicating the stream id, start, and end time allows to quickly skip over the tx blocks if only a specific stream is of interest.

The tx block content itself is structured as an indefinite-length array consisting of:

* an array of transactions with the following elements:
    
    * element with CBOR tag 6 (event) followed by an array of size 4:

        * unsigned integer denoting the id of the transaction
        * unsigned integer denoting the id of the generator creating this transaction
        * unsigned integer denoting the start time (time stamp)
        * unsigned integer denoting the end time (time stamp)

    * element with the CBOR tag 7, 8, and 9 (begin, record, end attribute) followed by an array of size 3

        * unsigned integer denoting the name (as dictionary id)
        * unsigned integer denoting the data type.
        * signed integer, unsigned integer or double denoting the value (depending on data type)

The data type is encoded as follows:

| id | name                         | C++/SystemC data type                                             | represented as            |
|----|------------------------------|-------------------------------------------------------------------|---------------------------|
|  0 | BOOLEAN                      | bool                                                              | unsigned int              |
|  1 | ENUMERATION                  | enum                                                              | unsigned int (string id)  |
|  2 | INTEGER                      | char, short, int, long, long long, sc_int, sc_bigint              | unsigned int              |
|  3 | UNSIGNED                     | unsigned [char, short, int, long, long long], sc_uint, sc_biguint | unsigned int              |
|  4 | FLOATING_POINT_NUMBER        | float, double                                                     | double                    |
|  5 | BIT_VECTOR                   | sc_bit, sc_bv                                                     | unsigned int (string id)  |
|  6 | LOGIC_VECTOR                 | sc_logic, sc_lv                                                   | unsigned int (string id)  |
|  7 | FIXED_POINT_INTEGER          | sc_fixed                                                          | double                    |
|  8 | UNSIGNED_FIXED_POINT_INTEGER | sc_ufixed                                                         | double                    |
|  9 | POINTER                      | void*                                                             | unsigned int              |
| 10 | STRING                       | string, std::string                                               | unsigned int (string id)  |
| 11 | TIME                         | sc_time                                                           | unsigned int (time stamp) |

## relationship chunk

The uncompressed relationship chunk is denoted be CBOR tag 14 followed by an [encoded CBOR data item](https://www.rfc-editor.org/rfc/rfc8949.html#embedded-di) holding the content.

The compressed relationship chunk is denoted by CBOR tag 15 followed by an array having 2 entries:

* unsigned integer denoting the uncompressed size of the   
* encoded CBOR data item (a byte string) holding the LZ4 compressed content of the relationship

The relationship content itself is structured as an indefinite-length array consisting of:

* array of size 5 denoting a relation and consisting of

    * unsigned integer denoting the name (id of string)
    * unsigned integer denoting the source tx id
    * unsigned integer denoting the sink tx id
    * unsigned integer denoting the source stream id
    * unsigned integer denoting the sink stream id
    

