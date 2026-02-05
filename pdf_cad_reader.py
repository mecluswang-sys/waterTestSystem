"""
CAD图纸PDF识别脚本
用于识别PC连接PLC设备的连接图
"""

import fitz  # PyMuPDF
from PIL import Image
import io
import sys

def extract_pdf_info(pdf_path):
    """提取PDF信息"""
    print(f"正在分析PDF文件: {pdf_path}\n")
    print("=" * 60)
    
    try:
        # 打开PDF
        doc = fitz.open(pdf_path)
        
        # 基本信息
        print(f"文档信息:")
        print(f"  页数: {len(doc)}")
        print(f"  标题: {doc.metadata.get('title', 'N/A')}")
        print(f"  作者: {doc.metadata.get('author', 'N/A')}")
        print(f"  创建工具: {doc.metadata.get('creator', 'N/A')}")
        print(f"  制作工具: {doc.metadata.get('producer', 'N/A')}")
        print("=" * 60)
        
        # 遍历每一页
        for page_num in range(len(doc)):
            page = doc[page_num]
            print(f"\n第 {page_num + 1} 页:")
            print("-" * 60)
            
            # 1. 提取文本
            text = page.get_text()
            if text.strip():
                print(f"\n提取的文本内容:")
                print(text)
            else:
                print("  (未找到可提取的文本)")
            
            # 2. 分析文本块（带位置信息）
            text_blocks = page.get_text("dict")["blocks"]
            text_items = []
            for block in text_blocks:
                if "lines" in block:
                    for line in block["lines"]:
                        for span in line["spans"]:
                            text_items.append({
                                "text": span["text"],
                                "size": round(span["size"], 1),
                                "font": span["font"],
                                "x": round(span["bbox"][0], 1),
                                "y": round(span["bbox"][1], 1)
                            })
            
            if text_items:
                print(f"\n文本详细信息 (共 {len(text_items)} 项):")
                for item in text_items[:50]:  # 只显示前50项
                    print(f"  位置({item['x']}, {item['y']}) | 大小{item['size']} | {item['text']}")
                if len(text_items) > 50:
                    print(f"  ... 还有 {len(text_items) - 50} 项")
            
            # 3. 分析图像
            image_list = page.get_images()
            print(f"\n图像数量: {len(image_list)}")
            
            # 4. 分析绘图对象
            drawings = page.get_drawings()
            print(f"绘图对象数量: {len(drawings)}")
            
            # 5. 导出页面为图像（用于可视化）
            print(f"\n正在导出第 {page_num + 1} 页为图像...")
            zoom = 2  # 放大倍数，提高清晰度
            mat = fitz.Matrix(zoom, zoom)
            pix = page.get_pixmap(matrix=mat)
            
            output_image = f"page_{page_num + 1}.png"
            pix.save(output_image)
            print(f"  已保存为: {output_image}")
            print(f"  图像尺寸: {pix.width} x {pix.height}")
        
        doc.close()
        
        print("\n" + "=" * 60)
        print("分析完成！")
        print("\n提示：")
        print("1. 已将每页导出为PNG图像，可以直接查看")
        print("2. 如果文本提取不完整，可能需要使用OCR技术")
        print("3. CAD图纸通常包含：设备名称、端口号、连接线路、技术参数等")
        
    except Exception as e:
        print(f"错误: {str(e)}")
        import traceback
        traceback.print_exc()

def analyze_plc_connection(pdf_path):
    """专门分析PLC连接信息"""
    print("\n" + "=" * 60)
    print("PLC连接信息分析")
    print("=" * 60)
    
    try:
        doc = fitz.open(pdf_path)
        
        # 关键词列表
        keywords = [
            'PC', 'PLC', 'COM', 'USB', 'Ethernet', 'RS232', 'RS485',
            'Port', '端口', 'IP', '波特率', 'Baud', '连接', 'Connection',
            '串口', '网口', 'RJ45', 'DB9', 'DB25', '通讯', '通信',
            '地址', 'Address', '设备', 'Device', '控制器', 'Controller'
        ]
        
        found_keywords = {}
        
        for page_num in range(len(doc)):
            page = doc[page_num]
            text = page.get_text()
            
            # 搜索关键词
            for keyword in keywords:
                if keyword.lower() in text.lower():
                    if keyword not in found_keywords:
                        found_keywords[keyword] = []
                    
                    # 提取包含关键词的上下文
                    lines = text.split('\n')
                    for i, line in enumerate(lines):
                        if keyword.lower() in line.lower():
                            context_start = max(0, i - 1)
                            context_end = min(len(lines), i + 2)
                            context = '\n'.join(lines[context_start:context_end])
                            found_keywords[keyword].append({
                                'page': page_num + 1,
                                'context': context
                            })
        
        if found_keywords:
            print("\n找到的关键信息:")
            for keyword, occurrences in found_keywords.items():
                print(f"\n关键词: {keyword} (出现 {len(occurrences)} 次)")
                for occ in occurrences[:3]:  # 只显示前3次出现
                    print(f"  第 {occ['page']} 页:")
                    print(f"    {occ['context'][:200]}")  # 限制长度
        else:
            print("\n未找到关键的连接信息（可能文本嵌入在图形中）")
            print("建议：查看导出的PNG图像文件")
        
        doc.close()
        
    except Exception as e:
        print(f"分析错误: {str(e)}")

if __name__ == "__main__":
    pdf_file = "水介质测试.pdf"
    
    # 提取基本信息
    extract_pdf_info(pdf_file)
    
    # 分析PLC连接
    analyze_plc_connection(pdf_file)
