import type { DashboardAgent } from './models';
export declare function isEnrollmentPending(agent: Pick<DashboardAgent, 'approved' | 'rejected'>): boolean;
export declare function canManageAgent(agent: Pick<DashboardAgent, 'approved' | 'rejected'>): boolean;
export declare function maintenanceAction(agent: Pick<DashboardAgent, 'maintenance'>): 'maintenance' | 'resume';
export declare function maintenanceActionLabel(agent: Pick<DashboardAgent, 'maintenance'>): string;
