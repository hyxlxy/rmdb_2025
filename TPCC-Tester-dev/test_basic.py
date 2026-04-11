#!/usr/bin/env python3
"""
Simple test to verify RMDB basic functionality
"""

import sys
import os

# Add the tpcc module to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'tpcc'))

from tpcc.database.database_connection import DatabaseConnection

def test_basic_operations():
    """Test basic RMDB operations."""
    print("Testing basic RMDB operations...")

    try:
        with DatabaseConnection("localhost", 8765) as db:
            # Test simple table creation
            with db.get_cursor() as cursor:
                cursor.execute("CREATE TABLE test_table (id int, name char(20))")
                print("✓ Table created successfully")

                # Test insert
                cursor.execute("INSERT INTO test_table VALUES (1, 'test')")
                print("✓ Insert successful")

                # Test select
                cursor.execute("SELECT * FROM test_table")
                result = cursor.fetchall()
                print(f"✓ Select successful: {result}")

                # Test count
                cursor.execute("SELECT COUNT(*) FROM test_table")
                count_result = cursor.fetchall()
                print(f"✓ Count successful: {count_result}")

        print("All basic operations passed!")

    except Exception as e:
        print(f"✗ Test failed: {e}")
        return False

    return True

if __name__ == "__main__":
    test_basic_operations()