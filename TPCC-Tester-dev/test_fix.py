#!/usr/bin/env python3
"""
测试脚本：验证修复后的consistency_checker是否工作正常
"""

import sys
import os

# 添加tpcc到路径
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'tpcc'))

from tpcc.database.database_connection import DatabaseConnection
from tpcc.executor.consistency_checker import ConsistencyCheckExecutor
from tpcc.database.schema_manager import SchemaManager
from tpcc.executor.load_executor import LoadExecutor

def test_consistency_checker():
    """测试consistency_checker的基本功能"""
    print("测试consistency_checker修复...")
    
    test_results = []
    
    try:
        # 创建数据库连接
        db = DatabaseConnection()
        db.connect()
        print("✓ 数据库连接成功")
        test_results.append(True)
        
        # 先初始化数据库和加载数据
        print("\n=== 初始化数据库和加载数据 ===")
        
        # 创建schema
        schema_manager = SchemaManager(db)
        try:
            schema_manager.create_schema()
            print("✓ 数据库schema创建成功")
            schema_manager.create_indexes()
            print("✓ 数据库索引创建成功")
            test_results.append(True)
        except Exception as e:
            print(f"✗ 数据库schema创建失败: {e}")
            test_results.append(False)
            return False
        
        # 加载数据
        load_executor = LoadExecutor(db)
        csv_data_dir = "/root/dbstart/rmdb/src/test/performance_test/table_data"
        
        if os.path.exists(csv_data_dir):
            try:
                print(f"从 {csv_data_dir} 加载CSV数据...")
                load_executor.load_all_data_csv(csv_data_dir)
                print("✓ 数据加载成功")
                test_results.append(True)
            except Exception as e:
                print(f"✗ 数据加载失败: {e}")
                test_results.append(False)
                return False
        else:
            print(f"✗ CSV数据目录不存在: {csv_data_dir}")
            test_results.append(False)
            return False
        
        # 创建consistency_checker实例
        checker = ConsistencyCheckExecutor(db, scale_factor=1)
        print("✓ ConsistencyCheckExecutor创建成功")
        test_results.append(True)
        
        # 测试get_database_stats方法
        print("\n=== 测试get_database_stats方法 ===")
        try:
            stats = checker.get_database_stats()
            print(f"✓ 数据库统计获取成功: {len(stats)} 个表")
            
            # 检查数据是否正常加载
            data_loaded = True
            for table, count in stats.items():
                print(f"  {table}: {count} 行")
                if count == 0 and table in ['customer', 'orders', 'order_line', 'history']:
                    print(f"    ⚠️  警告: {table} 表没有数据，可能数据加载失败")
                    data_loaded = False
            
            if not data_loaded:
                print("⚠️  数据加载可能有问题，建议检查数据加载过程")
                test_results.append(False)
            else:
                test_results.append(True)
                
        except Exception as e:
            print(f"✗ get_database_stats失败: {e}")
            test_results.append(False)
        
        # 测试基本的表计数检查
        print("\n=== 测试基本的表计数检查 ===")
        try:
            checks = checker._check_table_counts()
            print(f"✓ 表计数检查成功: {len(checks)} 个检查")
            
            # 检查每个检查的结果
            table_check_success = True
            for check_name, result in checks.items():
                status = '✓' if result else '✗'
                print(f"  {check_name}: {status}")
                if not result:
                    table_check_success = False
            
            test_results.append(table_check_success)
            
        except Exception as e:
            print(f"✗ 表计数检查失败: {e}")
            test_results.append(False)
        
        # 测试外键验证（简化版本）
        print("\n=== 测试外键验证 ===")
        try:
            checks = checker._validate_foreign_keys()
            print(f"✓ 外键验证成功: {len(checks)} 个检查")
            
            # 检查每个检查的结果
            fk_check_success = True
            for check_name, result in checks.items():
                status = '✓' if result else '✗'
                print(f"  {check_name}: {status}")
                if not result:
                    fk_check_success = False
            
            test_results.append(fk_check_success)
            
        except Exception as e:
            print(f"✗ 外键验证失败: {e}")
            test_results.append(False)
        
        # 测试业务规则验证（简化版本）
        print("\n=== 测试业务规则验证 ===")
        try:
            checks = checker._validate_business_rules()
            print(f"✓ 业务规则验证成功: {len(checks)} 个检查")
            
            # 检查每个检查的结果
            business_rule_success = True
            for check_name, result in checks.items():
                status = '✓' if result else '✗'
                print(f"  {check_name}: {status}")
                if not result:
                    business_rule_success = False
            
            test_results.append(business_rule_success)
            
        except Exception as e:
            print(f"✗ 业务规则验证失败: {e}")
            test_results.append(False)
        
        # 计算总体测试结果
        total_tests = len(test_results)
        passed_tests = sum(test_results)
        
        print(f"\n=== 测试结果汇总 ===")
        print(f"总测试数: {total_tests}")
        print(f"通过测试: {passed_tests}")
        print(f"失败测试: {total_tests - passed_tests}")
        
        if passed_tests == total_tests:
            print("\n✓ 所有测试通过！consistency_checker修复成功")
            return True
        else:
            print(f"\n⚠️  有 {total_tests - passed_tests} 个测试失败")
            print("建议检查数据加载和数据库状态")
            return False
        
    except Exception as e:
        print(f"✗ 测试失败: {e}")
        return False
    finally:
        if 'db' in locals():
            db.close()

if __name__ == "__main__":
    success = test_consistency_checker()
    sys.exit(0 if success else 1)
