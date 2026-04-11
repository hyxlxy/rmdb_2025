#!/usr/bin/env python3
"""
Simple script to run complete TPC-C test
"""

import sys
import os

# Add the tpcc module to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'tpcc'))

from tpcc.main import main

if __name__ == "__main__":
    # Set default arguments for complete test
    sys.argv = [
        'run_complete_test.py',
        '--complete-test',
        '--scale', '1',
        '--threads', '4',
        '--transactions', '100',
        '--csv-data-dir', '/home/hxl/Desktop/rmdb_2025/src/test/performance_test/table_data'
    ]
    
    print("Starting complete TPC-C test...")
    print("Arguments:", sys.argv)
    print("=" * 60)
    
    main()
