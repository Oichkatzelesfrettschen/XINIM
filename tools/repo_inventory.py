#!/usr/bin/env python3
"""
XINIM Repository Inventory Tool
Analyzes and reports on repository structure and contents
"""

import os
import sys
import json
import hashlib
from pathlib import Path
from collections import defaultdict
from datetime import datetime

class RepositoryInventory:
    def __init__(self, root_path='.'):
        self.root = Path(root_path).resolve()
        self.stats = defaultdict(int)
        self.issues = []
        self.inventory = {}
        
    def scan_directory(self, directory):
        """Scan a directory and collect statistics"""
        items = []
        for item in directory.iterdir():
            if item.name.startswith('.'):
                continue
            if item.name in ['build', 'cleanup_backups', '.xmake']:
                continue
                
            if item.is_file():
                file_info = self.analyze_file(item)
                items.append(file_info)
            elif item.is_dir():
                subdir_info = {
                    'name': item.name,
                    'type': 'directory',
                    'path': str(item.relative_to(self.root)),
                    'contents': self.scan_directory(item)
                }
                items.append(subdir_info)
        return items
    
    def analyze_file(self, file_path):
        """Analyze a single file"""
        rel_path = file_path.relative_to(self.root)
        extension = file_path.suffix
        
        # Update statistics
        self.stats['total_files'] += 1
        self.stats[f'files_{extension}'] += 1
        
        # Check for issues
        if ' 2.' in file_path.name:
            self.issues.append(f"Duplicate file pattern: {rel_path}")
            self.stats['duplicate_files'] += 1
        
        if any(term in file_path.name.lower() for term in 
               ['synthesis', 'transcendent', 'omnipotent', 'revolutionary', 'quantum']):
            self.issues.append(f"AI jargon in filename: {rel_path}")
            self.stats['jargon_files'] += 1
            
        # Get file info
        try:
            size = file_path.stat().st_size
            self.stats['total_size'] += size
            
            return {
                'name': file_path.name,
                'type': 'file',
                'path': str(rel_path),
                'extension': extension,
                'size': size,
                'lines': self.count_lines(file_path) if self.is_text_file(file_path) else 0
            }
        except Exception as e:
            self.issues.append(f"Error reading {rel_path}: {e}")
            return {
                'name': file_path.name,
                'type': 'file',
                'path': str(rel_path),
                'error': str(e)
            }
    
    def is_text_file(self, file_path):
        """Check if file is likely a text file"""
        text_extensions = {'.cpp', '.c', '.hpp', '.h', '.py', '.sh', '.lua', 
                          '.md', '.txt', '.cmake', '.json', '.yml', '.yaml'}
        return file_path.suffix in text_extensions
    
    def count_lines(self, file_path):
        """Count lines in a text file"""
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                lines = sum(1 for _ in f)
                self.stats['total_lines'] += lines
                return lines
        except:
            return 0
    
    def generate_report(self):
        """Generate inventory report"""
        print("=" * 70)
        print("XINIM REPOSITORY INVENTORY REPORT")
        print("=" * 70)
        print(f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print(f"Repository Root: {self.root}")
        print()
        
        # Scan repository
        print("Scanning repository...")
        self.inventory = self.scan_directory(self.root)
        
        # Statistics
        print("\n📊 STATISTICS")
        print("-" * 40)
        print(f"Total Files: {self.stats['total_files']}")
        print(f"Total Size: {self.stats['total_size'] / (1024*1024):.2f} MB")
        print(f"Total Lines of Code: {self.stats['total_lines']:,}")
        print()
        
        # File type breakdown
        print("📁 FILE TYPES")
        print("-" * 40)
        extensions = [(k.replace('files_', ''), v) for k, v in self.stats.items() 
                     if k.startswith('files_')]
        extensions.sort(key=lambda x: x[1], reverse=True)
        for ext, count in extensions[:10]:
            if ext:
                print(f"  {ext:10} {count:5} files")
        print()
        
        # Component analysis
        print("🔧 COMPONENTS")
        print("-" * 40)
        components = {
            'kernel': 0,
            'commands': 0,
            'tests': 0,
            'crypto': 0,
            'fs': 0,
            'lib': 0,
            'mm': 0
        }
        
        for item in self.walk_inventory(self.inventory):
            if item['type'] == 'file':
                path = item['path']
                for component in components:
                    if path.startswith(component + '/'):
                        components[component] += 1
                        break
        
        for component, count in sorted(components.items()):
            print(f"  {component:15} {count:5} files")
        print()
        
        # Issues found
        if self.issues:
            print("⚠️  ISSUES FOUND")
            print("-" * 40)
            print(f"Duplicate Files: {self.stats.get('duplicate_files', 0)}")
            print(f"Files with AI Jargon: {self.stats.get('jargon_files', 0)}")
            
            if len(self.issues) <= 20:
                print("\nDetailed Issues:")
                for issue in self.issues:
                    print(f"  • {issue}")
            else:
                print(f"\nShowing first 20 of {len(self.issues)} issues:")
                for issue in self.issues[:20]:
                    print(f"  • {issue}")
        else:
            print("✅ NO ISSUES FOUND")
        
        print("\n" + "=" * 70)
    
    def walk_inventory(self, items):
        """Walk through inventory items recursively"""
        for item in items:
            if item['type'] == 'file':
                yield item
            elif item['type'] == 'directory':
                yield from self.walk_inventory(item.get('contents', []))
    
    def save_json(self, output_file='inventory.json'):
        """Save inventory as JSON"""
        with open(output_file, 'w') as f:
            json.dump({
                'timestamp': datetime.now().isoformat(),
                'root': str(self.root),
                'stats': dict(self.stats),
                'issues': self.issues,
                'inventory': self.inventory
            }, f, indent=2)
        print(f"Inventory saved to {output_file}")

def main():
    inventory = RepositoryInventory()
    inventory.generate_report()
    
    if '--json' in sys.argv:
        inventory.save_json()

if __name__ == '__main__':
    main()