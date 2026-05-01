import type { DashboardAgent } from './models';

export function isEnrollmentPending(agent: Pick<DashboardAgent, 'approved' | 'rejected'>): boolean {
  return !agent.approved && !agent.rejected;
}

export function canManageAgent(agent: Pick<DashboardAgent, 'approved' | 'rejected'>): boolean {
  return agent.approved && !agent.rejected;
}

export function maintenanceAction(agent: Pick<DashboardAgent, 'maintenance'>): 'maintenance' | 'resume' {
  return agent.maintenance ? 'resume' : 'maintenance';
}

export function maintenanceActionLabel(agent: Pick<DashboardAgent, 'maintenance'>): string {
  return agent.maintenance ? 'Resume' : 'Maintenance';
}
