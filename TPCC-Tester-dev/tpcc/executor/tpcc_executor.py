"""
TPC-C benchmark executor.
Refactored to provide clean separation of concerns and better maintainability.
"""

import logging
from typing import Dict, Any

from ..database.database_connection import DatabaseConnection
from ..database.schema_manager import SchemaManager
from ..data_generator.tpcc_generator import TpccDataGenerator
from ..models import *
from .load_executor import LoadExecutor
from .consistency_checker import ConsistencyCheckExecutor
from .transaction_executor import TransactionExecutor, BenchmarkResult

logger = logging.getLogger(__name__)


class TpccExecutor:
    """Main executor for TPC-C benchmark operations."""

    def __init__(self, db_connection: DatabaseConnection, scale_factor: int = 1):
        """Initialize TPC-C executor.

        Args:
            db_connection: Database connection instance
            scale_factor: Number of warehouses to test
        """
        self.db = db_connection
        self.scale_factor = scale_factor
        self.schema_manager = SchemaManager(db_connection)
        self.data_generator = TpccDataGenerator(scale_factor)
        self.load_executor = LoadExecutor(db_connection)
        self.consistency_checker = ConsistencyCheckExecutor(db_connection, scale_factor)
        self.transaction_executor = TransactionExecutor(db_connection, scale_factor)

    def initialize_database(self) -> None:
        """Initialize database with schema and data."""
        logger.info("Initializing TPC-C database...")

        # Create schema
        self.schema_manager.create_schema()

        # Skip index creation for now due to RMDB index bug
        # self.schema_manager.create_indexes()
        logger.info("Skipping index creation due to known issues")

        # We not check Validate schema now
        # if not self.schema_manager.validate_schema():
        #     raise RuntimeError("Schema validation failed")

        logger.info("Database initialized successfully")

    def load_data(self) -> None:
        """Load TPC-C data into database."""
        logger.info("Loading TPC-C data...")

        data_generators = self.data_generator.generate_all_data()
        self.load_executor.load_all_data(data_generators)

        logger.info("TPC-C data loaded successfully")

    def load_data_from_csv(self, csv_data_dir: str) -> None:
        """Load TPC-C data from existing CSV files using RMDB's LOAD command.
        
        Args:
            csv_data_dir: Directory containing CSV data files
        """
        logger.info(f"Loading TPC-C data from CSV files in {csv_data_dir}")
        self.load_executor.load_all_data_csv(csv_data_dir)
        logger.info("TPC-C data loaded successfully from CSV files")

    def run_consistency_checks(self) -> Dict[str, bool]:
        """Run consistency checks on loaded data."""
        return self.consistency_checker.run_consistency_checks()

    def get_database_stats(self) -> Dict[str, Any]:
        """Get database statistics."""
        return self.consistency_checker.get_database_stats()

    def run_benchmark(
        self,
        num_threads: int = 1,
        transactions_per_thread: int = 100,
        read_write_ratio: float = 0.5,
        duration_seconds: int = 0,
        **kwargs,
    ) -> BenchmarkResult:
        """Run TPC-C benchmark with transaction execution.

        Args:
            num_threads: Number of concurrent threads
            transactions_per_thread: Number of transactions per thread
            read_write_ratio: Ratio of read-write vs read-only transactions
            duration_seconds: Duration of benchmark in seconds
            **kwargs: Additional arguments for transaction executor

        Returns:
            BenchmarkResult with performance metrics
        """
        logger.info(f"Starting TPC-C benchmark with {num_threads} threads")

        # Calculate transactions based on duration if provided
        if duration_seconds > 0:
            # Estimate transactions based on TPS and duration
            estimated_tps = 50  # Conservative estimate
            total_transactions = estimated_tps * duration_seconds
            transactions_per_thread = max(1, total_transactions // num_threads)

        return self.transaction_executor.run_concurrent_benchmark(
            num_threads=num_threads,
            transactions_per_thread=transactions_per_thread,
            read_write_ratio=read_write_ratio,
            **kwargs,
        )

    def run_complete_test(
        self,
        csv_data_dir: str = None,
        num_threads: int = 4,
        transactions_per_thread: int = 100,
        read_write_ratio: float = 0.5,
        run_consistency_check: bool = True,
        run_benchmark: bool = True,
        show_stats: bool = True
    ) -> Dict[str, Any]:
        """Run complete TPC-C test including initialization, data loading, consistency check, and benchmark.
        
        Args:
            csv_data_dir: Directory containing CSV data files (if None, generate data)
            num_threads: Number of concurrent threads for benchmark
            transactions_per_thread: Number of transactions per thread
            read_write_ratio: Ratio of read-write vs read-only transactions
            run_consistency_check: Whether to run consistency checks
            run_benchmark: Whether to run performance benchmark
            show_stats: Whether to show database statistics
            
        Returns:
            Dictionary containing test results
        """
        results = {}
        
        try:
            # Step 1: Initialize database schema
            logger.info("=== Step 1: Initializing Database Schema ===")
            self.initialize_database()
            results['schema_initialized'] = True
            
            # Step 2: Load data
            logger.info("=== Step 2: Loading Data ===")
            if csv_data_dir:
                # Use existing CSV files
                self.load_data_from_csv(csv_data_dir)
                results['data_loaded_from_csv'] = True
                results['csv_data_dir'] = csv_data_dir
            else:
                # Generate and load data
                self.load_data()
                results['data_generated_and_loaded'] = True
            
            # Step 3: Show database statistics
            if show_stats:
                logger.info("=== Step 3: Database Statistics ===")
                stats = self.get_database_stats()
                results['database_stats'] = stats
                for table, count in stats.items():
                    logger.info(f"  {table}: {count:,}")
            
            # Step 4: Run consistency checks
            if run_consistency_check:
                logger.info("=== Step 4: Running Consistency Checks ===")
                checks = self.run_consistency_checks()
                results['consistency_checks'] = checks
                if not all(checks.values()):
                    logger.error("Some consistency checks failed")
                    results['consistency_check_passed'] = False
                else:
                    logger.info("All consistency checks passed")
                    results['consistency_check_passed'] = True
            
            # Step 5: Run performance benchmark
            if run_benchmark:
                logger.info("=== Step 5: Running Performance Benchmark ===")
                benchmark_result = self.run_benchmark(
                    num_threads=num_threads,
                    transactions_per_thread=transactions_per_thread,
                    read_write_ratio=read_write_ratio
                )
                results['benchmark_result'] = benchmark_result
                
                # Print benchmark results
                logger.info("TPC-C CONCURRENT BENCHMARK RESULTS")
                logger.info("=" * 60)
                logger.info(f"Configuration: {num_threads} threads, {transactions_per_thread} transactions/thread")
                logger.info(f"Scale Factor: {self.scale_factor} warehouses")
                logger.info(f"Read-Write Ratio: {read_write_ratio}")
                logger.info(f"Total Transactions: {benchmark_result.total_transactions:,}")
                logger.info(f"Successful: {benchmark_result.successful_transactions:,}")
                logger.info(f"Failed: {benchmark_result.failed_transactions:,}")
                logger.info(f"Success Rate: {(benchmark_result.successful_transactions / benchmark_result.total_transactions) * 100:.2f}%")
                logger.info(f"Total Duration: {benchmark_result.total_duration:.2f} seconds")
                logger.info(f"Average Response Time: {benchmark_result.avg_response_time * 1000:.2f} ms")
                logger.info(f"Throughput: {benchmark_result.throughput_tps:.2f} TPS")
            
            logger.info("=== Complete TPC-C Test Finished Successfully ===")
            results['test_completed'] = True
            
        except Exception as e:
            logger.error(f"Test failed: {e}")
            results['test_completed'] = False
            results['error'] = str(e)
            raise
        
        return results
