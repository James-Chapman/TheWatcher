import type { UserRole } from './models';
export type DashboardView = 'monitoring' | 'agents' | 'alerts' | 'users' | 'maintenance' | 'silences' | 'settings' | 'views';
export interface NavItem {
    view: DashboardView;
    label: string;
    requiresOperator?: boolean;
    requiresGlobalOperator?: boolean;
}
export declare const NAV_ITEMS: NavItem[];
export declare function accountTypeLabel(role: UserRole): string;
export type MarkdownBlock = {
    type: 'heading';
    level: 1 | 2 | 3;
    text: string;
} | {
    type: 'paragraph';
    text: string;
} | {
    type: 'list';
    items: string[];
} | {
    type: 'code';
    text: string;
};
export declare function parseMarkdownBlocks(markdown: string): MarkdownBlock[];
export declare function safeMarkdownHref(href: string): string | null;
