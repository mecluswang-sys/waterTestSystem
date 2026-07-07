#!/usr/bin/env python
# -*- coding: utf-8 -*-
import pdfplumber
import sys
import io

# 设置标准输出为UTF-8
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

pdf_path = r"c:\Users\mickey\Documents\cpp\waterTestSystem\docs\水介质电磁阀测试系统的研究_曹洋.pdf"

try:
    print("打开PDF文件...")
    with pdfplumber.open(pdf_path) as pdf:
        print("文件成功打开")
        print(f"总页数: {len(pdf.pages)}\n")
        
        # 提取所有文本
        full_text = ""
        for i, page in enumerate(pdf.pages):
            print(f"正在提取第 {i+1}/{len(pdf.pages)} 页...")
            text = page.extract_text()
            if text:
                full_text += f"\n\n========== 第{i+1}页 ==========\n"
                full_text += text
        
        # 输出文本
        print("\n" + "="*80)
        print("PDF 内容")
        print("="*80 + "\n")
        print(full_text)
        
except Exception as e:
    import traceback
    print(f"错误: {e}")
    traceback.print_exc()
