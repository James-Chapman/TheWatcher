import type { AgentRecord, DashboardAgent, HealthColor, MetricsSnapshot } from './models';
export declare function classifyPercent(value: number): HealthColor;
export declare function worstColor(colors: HealthColor[]): HealthColor;
export declare function formatBytes(bytes: number): string;
export declare function formatDuration(seconds: number): string;
export declare function toDashboardAgents(agents: AgentRecord[], snapshots: MetricsSnapshot[], now?: number): DashboardAgent[];
export declare function agentStatus(agent: DashboardAgent): HealthColor;
export declare function summaryCounts(agents: DashboardAgent[]): Record<HealthColor, number>;
