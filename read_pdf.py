#!/usr/bin/env python
# -*- coding: utf-8 -*-
import os
import sys

pdf_path = r"c:\Users\mickey\Documents\cpp\waterTestSystem\docs\需求2——水用电磁阀测试设备方案（附带气检和吹气）.pdf"

try:
    # 检查文件是否存在
    if not os.path.exists(pdf_path):
        print(f"❌ 文件不存在: {pdf_path}")
        sys.exit(1)
    
    print(f"✓ 文件大小: {os.path.getsize(pdf_path)} 字节")
    print("\n=== 尝试读取PDF文本内容 ===\n")
    
    with open(pdf_path, 'rb') as f:
        # 读取整个文件
        content = f.read()
        
        # 尝试解码为UTF-8和其他编码
        try:
            text = content.decode('utf-8', errors='ignore')
        except:
            text = content.decode('latin-1', errors='ignore')
        
        # 过滤可打印字符
        printable_text = ''.join(c if ord(c) >= 32 or c in '\n\r\t' else ' ' for c in text)
        
        # 查找并提取中文内容
        lines = printable_text.split('\n')
        chinese_lines = [line.strip() for line in lines if any('\u4e00' <= c <= '\u9fff' for c in line)]
        
        print("=== 提取的中文内容 (前100行) ===\n")
        for i, line in enumerate(chinese_lines[:100]):
            if line:
                print(f"{i+1:3d}: {line}")
        
        print(f"\n总计找到 {len(chinese_lines)} 行中文内容")
        
except Exception as e:
    import traceback
    print(f"❌ 错误: {e}")
    traceback.print_exc()
