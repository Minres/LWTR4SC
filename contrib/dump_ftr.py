#!/usr/bin/env python3
##

import os
import sys
from optparse import OptionParser
import logging
from cbor2 import load, loads
import lz4.block

logger = logging.getLogger(__name__)

def dump_ftr(file_name_input):
    strings = {}

    def tag_cb(decoder, tag):
        def tx_tag_cb(decoder, txtag):
            if txtag.tag == 6:
                # id, generator id, start time, end time
                print(f"trans id:{txtag.value[0]}, gen:{txtag.value[1]}, start:{txtag.value[2]}, end:{txtag.value[3]}")
            elif txtag.tag == 7: # begin attr
                attr_name = strings[txtag.value[0]]
                if txtag.value[1] in [12, 1]:
                    print(f"  battr {attr_name}, type_id:{txtag.value[1]}, value:{strings[txtag.value[2]]}")
                else:
                    print(f"  battr {attr_name}, type_id:{txtag.value[1]}, value:{txtag.value[2]}")
            elif txtag.tag == 8: # record attr
                attr_name = strings[txtag.value[0]]
                if txtag.value[1] in [12, 1]:
                    print(f"  rattr {attr_name}, type_id:{txtag.value[1]}, value:{strings[txtag.value[2]]}")
                else:
                    print(f"  rattr {attr_name}, type_id:{txtag.value[1]}, value:{txtag.value[2]}")
            elif txtag.tag == 9: # end attr
                attr_name = strings[txtag.value[0]]
                if txtag.value[1] in [12, 1]:
                    print(f"  eattr {attr_name}, type_id:{txtag.value[1]}, value:{strings[txtag.value[2]]}")
                else:
                    print(f"  eattr {attr_name}, type_id:{txtag.value[1]}, value:{txtag.value[2]}")
            else:
                print(f"Found unknown entry: {txtag.tag}")
            return None
    
        if tag.tag == 6:
            logger.debug("Found info chunk")
        elif tag.tag in [8, 9]:
            strings.update(loads(tag.value if tag.tag==8 else lz4.block.decompress(tag.value[1], uncompressed_size=tag.value[0])))
            logger.debug("Found dictionary chunk")
        elif tag.tag in [10, 11]:
            direct = loads(tag.value if tag.tag==10 else lz4.block.decompress(tag.value[1], uncompressed_size=tag.value[0]), tag_hook=tag_cb)
            logger.debug("Found directory chunk")
        elif tag.tag in [12, 13]:
            tx_stream_id = tag.value[0]
            txchunk = loads(tag.value[1] if tag.tag==12 else lz4.block.decompress(tag.value[2], uncompressed_size=tag.value[1]), tag_hook=tx_tag_cb)
            logger.debug(f"Found tx chunk of stream id {tag.value[0]}")
        elif tag.tag in [14, 15]:
            relations = loads(tag.value if tag.tag==14 else lz4.block.decompress(tag.value[1], uncompressed_size=tag.value[0]))
            logger.debug("Found relationship chunk")
        elif tag.tag == 16:
            print(f"stream id:{tag.value[0]}, name:{strings[tag.value[1]]}, kind;:{strings[tag.value[2]]}")
        elif tag.tag == 17:
            print(f"generator id:{tag.value[0]}, name:{strings[tag.value[1]]}, stream:{tag.value[2]}")
        else:
            print(f"Found unknown entry: {tag.tag}")
        return None
    
    with open(file_name_input, 'rb') as fp:
        obj = load(fp, tag_hook=tag_cb)
        
        
if __name__== "__main__":    
    parser = OptionParser()
    parser.add_option("-v", "--verbose", action="store_true", dest="verbose", 
                      help="set output verbosity", default=False, metavar="")
    (options, args)  = parser.parse_args()
    logging.basicConfig(format='%(levelname)s: %(message)s', level=logging.DEBUG if options.verbose  else logging.INFO)
    for arg in args:
        dump_ftr(arg)
    